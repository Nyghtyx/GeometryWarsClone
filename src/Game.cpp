#include "Game.h"

#include <iostream>
#include <fstream>
#include <math.h>
#include <random>

Game::Game(const std::string& config)
{
    init(config);
}

void Game::init(const std::string& path)
{
    // read in config file here
    std::ifstream fin("config.txt");
    std::string temp;
    int wWidth{}, wHeight{};

    while (fin >> temp)
    {
        if (temp == "Window")
        {
            int wFramerateLimit{}, wFullScreen{};
            fin >> wWidth >> wHeight >> wFramerateLimit >> wFullScreen;
            if (wFullScreen)
            {
                m_window.create(sf::VideoMode(wWidth, wHeight), "Geometric Wars", sf::Style::Fullscreen);
            }
            else
            {
                m_window.create(sf::VideoMode(wWidth, wHeight), "Geometric Wars");
            }
            m_window.setFramerateLimit(wFramerateLimit);
        }
        else if (temp == "Font")
        {
            std::string fontFilename;
            int fontSize{};
            int fontColor[3]{};
            fin >> fontFilename >> fontSize;
            for (int i = 0; i < 3; i++)
            {
                fin >> fontColor[i];
            }

            if (!m_font.loadFromFile(fontFilename))
            {
                std::cerr << "Could not load font!\n";
                exit(-1);
            }
            m_text = sf::Text("Score : " + std::to_string(m_score), m_font, fontSize);
            m_text.setFillColor(sf::Color(fontColor[0], fontColor[1], fontColor[2]));
            m_text.setPosition(0.0f, 0.0f);
        }
        else if (temp == "Player")
        {
            fin >> m_playerConfig.SR >> m_playerConfig.CR >> m_playerConfig.S >> m_playerConfig.FR;
            fin >> m_playerConfig.FG >> m_playerConfig.FB >> m_playerConfig.OR >> m_playerConfig.OG;
            fin >> m_playerConfig.OB >> m_playerConfig.OT >> m_playerConfig.V;
        }
        else if (temp == "Enemy")
        {
            fin >> m_enemyConfig.SR >> m_enemyConfig.CR >> m_enemyConfig.SMIN >> m_enemyConfig.SMAX;
            fin >> m_enemyConfig.OR >> m_enemyConfig.OG >> m_enemyConfig.OB >> m_enemyConfig.OT;
            fin >> m_enemyConfig.VMIN >> m_enemyConfig.VMAX >> m_enemyConfig.L >> m_enemyConfig.SP;
        }
        else if (temp == "Bullet")
        {
            fin >> m_bulletConfig.SR >> m_bulletConfig.CR >> m_bulletConfig.S >> m_bulletConfig.FR;
            fin >> m_bulletConfig.FG >> m_bulletConfig.FB >> m_bulletConfig.OR >> m_bulletConfig.OG;
            fin >> m_bulletConfig.OB >> m_bulletConfig.OT >> m_bulletConfig.V >> m_bulletConfig.L;
        }
    }

    // Initialize random number distributions
    m_xDist = std::uniform_real_distribution<float>(m_enemyConfig.SR, wWidth - m_enemyConfig.SR);
    m_yDist = std::uniform_real_distribution<float>(m_enemyConfig.SR, wHeight - m_enemyConfig.SR);
    m_verticesDist = std::uniform_int_distribution<int>(m_enemyConfig.VMIN, m_enemyConfig.VMAX);
    m_speedDist = std::uniform_real_distribution<float>(m_enemyConfig.SMIN, m_enemyConfig.SMAX);
    m_angleDist = std::uniform_real_distribution<float>(0.0f, 2.0f * 3.141592f); // [0, 2pi]
    m_colorDist = std::uniform_int_distribution<int>(0, 255);
    m_random = std::uniform_real_distribution<float>(0.0f, 1.0f);


    ImGui::SFML::Init(m_window);

    // scale the imgui ui and text size by 2
    ImGui::GetStyle().ScaleAllSizes(2.0f);
    ImGui::GetIO().FontGlobalScale = 2.0f;

    spawnPlayer();
}

std::shared_ptr<Entity> Game::player()
{
    auto& players = m_entities.getEntities("player");
    //assert(players.size() == 1);
    return players.front();
}

void Game::run()
{
    while (m_running)
    {
        // update the entity manager
        m_entities.update();

        // required update call to imgui
        ImGui::SFML::Update(m_window, m_deltaClock.restart());

        if (m_spawning) { sEnemySpawner(); sSmallAllyBulletSpawner(); }
        if(m_lifespan) { sLifespan(); }
        if(m_movement) { sMovement(); }
        if(m_collision) { sCollision(); }
        if(m_cooldown) { sCooldown(); }
        sUserInput();
        sGUI();
        sRender();
        
        if (!m_paused)
        {
            m_currentFrame++;
        }
    }
}

void Game::setPaused(bool paused)
{
    m_spawning = !paused;
    m_lifespan = !paused;
    m_movement = !paused;
    m_collision = !paused;
    m_cooldown = !paused;
}

// respawn the player in the middle of the screen
void Game::spawnPlayer()
{
    // We create every entity by calling EntityManager.addEntity(tag)
    auto entity = m_entities.addEntity("player");

    // Give this entity a Transform so it spawns at (200,200) with velocity (1,1) and angle 0.0f
    entity->add<CTransform>(Vec2f(m_window.getSize().x / 2, m_window.getSize().y / 2), Vec2f(0.0f, 0.0f), 0.0f);

    entity->add<CShape>(m_playerConfig.SR, m_playerConfig.V, sf::Color(m_playerConfig.FR, m_playerConfig.FG, m_playerConfig.FB), 
        sf::Color(m_playerConfig.OR, m_playerConfig.OG, m_playerConfig.OB), m_playerConfig.OT);
    entity->get<CShape>().circle.setOrigin(m_playerConfig.SR, m_playerConfig.SR);

    // Add an input component to the player so that we can use inputs
    entity->add<CInput>();

    // Add special move
    // Cooldown in frames = cooldown in min * 60 * fps 
    entity->add<CSpecial>(1*60*60);
    entity->get<CSpecial>().text = sf::Text("Special Move Available!", m_font, 24);
    entity->get<CSpecial>().text.setFillColor(sf::Color(255, 255, 255));
    entity->get<CSpecial>().text.setPosition(200.0f, 0.0f);
}

// spawn an enemy at a random position
void Game::spawnEnemy()
{
    // the enemy must be spawned completely within the bounds of the window
    int vertices = m_verticesDist(m_randomGen);
    // speed between min and max
    float speed = m_speedDist(m_randomGen);
    // random angle between [0, 2pi] for direction
    float theta = m_angleDist(m_randomGen);
    float speedX = speed * std::cos(theta);
    float speedY = speed * std::sin(theta);

    auto entity = m_entities.addEntity("enemy");
    entity->add<CTransform>(Vec2f(m_xDist(m_randomGen), m_yDist(m_randomGen)), Vec2f(speedX, speedY), 0.0f);
    entity->add<CShape>(m_enemyConfig.SR, vertices,
        sf::Color(m_colorDist(m_randomGen), m_colorDist(m_randomGen), m_colorDist(m_randomGen)),
        sf::Color(m_enemyConfig.OR, m_enemyConfig.OG, m_enemyConfig.OB), m_enemyConfig.OT);
    entity->get<CShape>().circle.setOrigin(m_enemyConfig.SR, m_enemyConfig.SR);
    entity->add<CScore>(vertices * 100);
}

// spawns the small enemies when a big one (input entity e) explodes
void Game::spawnSmallEnemies(std::shared_ptr<Entity> e)
{
    // - spawn a number of small enemies equal to the vertices of the original enemy
    // - set each small enemy to the same color as the original, half the size
    // - small enemies are worth double points of the original enemy
    int vertices = (int)e->get<CShape>().circle.getPointCount();
    float theta =  m_angleDist(m_randomGen);
    for (int i = 0; i < vertices; i++)
    {
        
        auto entity = m_entities.addEntity("smallEnemy");
        entity->add<CTransform>(e->get<CTransform>().pos, 
            Vec2f(std::cos(theta + 2.0f * 3.141592f / vertices * i), std::sin(theta + 2.0f * 3.141592f / vertices * i)) 
            * e->get<CTransform>().velocity.length(),
            0.0f);
        entity->add<CShape>(m_enemyConfig.SR / 2, vertices,
            e->get<CShape>().circle.getFillColor(),
            e->get<CShape>().circle.getOutlineColor(), m_enemyConfig.OT);
        entity->get<CShape>().circle.setOrigin(m_enemyConfig.SR / 2.0f, m_enemyConfig.SR / 2.0f);
        entity->add<CScore>(vertices * 200);
        entity->add<CLifespan>(m_enemyConfig.L);
    }
}

// spawns a bullet from a given entity to a target location
void Game::spawnBullet(std::shared_ptr<Entity> entity, const Vec2f& target)
{
    Vec2f entityPos = entity->get<CTransform>().pos;
    Vec2f bulletSpeed = (target - entityPos) / target.dist(entityPos) * m_bulletConfig.S;

    auto bullet = m_entities.addEntity("bullet");
    bullet->add<CTransform>(entityPos, bulletSpeed, 0.0f);
    bullet->add<CShape>(m_bulletConfig.SR, m_bulletConfig.V, sf::Color(m_bulletConfig.FR, m_bulletConfig.FG, m_bulletConfig.FB),
        sf::Color(m_bulletConfig.OR, m_bulletConfig.OG, m_bulletConfig.OB), m_bulletConfig.OT);
    bullet->get<CShape>().circle.setOrigin(m_bulletConfig.SR, m_bulletConfig.SR);
    bullet->add<CLifespan>(m_bulletConfig.L);
}

// spawns a small ally that orbits the player (input entity e) and randomly shoots bullets
void Game::spawnSpecialWeapon(std::shared_ptr<Entity> e)
{
    // - spawn a number of small allies equal to the vertices of the player
    // - the allies start at the center of the player and move outwards until 3 x radius of the player
    // - then start spinning around the player shooting bullets at random directions
    if (e->has<CSpecial>() && e->get<CSpecial>().available)
    {
        int vertices = (int)e->get<CShape>().circle.getPointCount();
        float theta = m_angleDist(m_randomGen);
        for (int i = 0; i < vertices; i++)
        {

            auto entity = m_entities.addEntity("smallAlly");
            entity->add<CTransform>(e->get<CTransform>().pos,
                Vec2f(std::cos(theta + 2.0f * 3.141592f / vertices * i), std::sin(theta + 2.0f * 3.141592f / vertices * i))
                * m_playerConfig.S,
                0.0f);
            entity->add<CShape>(m_playerConfig.SR / 2, vertices,
                e->get<CShape>().circle.getFillColor(),
                e->get<CShape>().circle.getOutlineColor(), m_playerConfig.OT);
            entity->get<CShape>().circle.setOrigin(m_playerConfig.SR / 2.0f, m_playerConfig.SR / 2.0f);
        }
        e->get<CSpecial>().lastfired = m_currentFrame;
        e->get<CSpecial>().available = false;
        e->get<CSpecial>().text.setString("Special Move on Cooldown!");
    }
}

void Game::sMovement()
{
    auto& playerTransform = player()->get<CTransform>();
    // handle player movement
    Vec2f tempVel = Vec2f(0.0f, 0.0f);
    auto& playerInput = player()->get<CInput>();
    if (playerInput.up) { tempVel.y += -1.0f; }
    if (playerInput.down) { tempVel.y += 1.0f; }
    if (playerInput.left) { tempVel.x += -1.0f; }
    if (playerInput.right) { tempVel.x += 1.0f; }

    if (tempVel.x == 0.0f && tempVel.y == 0.0f)
    {
        playerTransform.velocity = Vec2f(0.0f, 0.0f);
    }
    else
    {
        playerTransform.velocity = tempVel / tempVel.length() * m_playerConfig.S;
    }

    for (auto& e : m_entities.getEntities())
    {
        auto& transform = e->get<CTransform>();
        transform.pos += transform.velocity;
        
        // special small Ally movement properties
        if (e->tag() == "smallAlly")
        {
            // once they are at 4x distance from player they stop moving outwards
            if (transform.pos.dist(playerTransform.pos) >= 3.9f * m_playerConfig.SR)
            {
                transform.velocity = Vec2f(0.0f, 0.0f);
                float currentAngle = std::atan2(transform.pos.y - playerTransform.pos.y, transform.pos.x - playerTransform.pos.x);
                transform.pos = playerTransform.pos + Vec2f(std::cos(currentAngle + 0.02f), std::sin(currentAngle + 0.02f)) * 4.2f * m_playerConfig.SR;
            }
            // they move with the player
            transform.pos += playerTransform.velocity;
        }
        e->get<CShape>().circle.setPosition(transform.pos);
        transform.angle += 1.0f;
        e->get<CShape>().circle.setRotation(transform.angle);
    }
}

void Game::sLifespan()
{
    // for all entities
    //       if entity has no lifespan component, skip it
    //       if entity has > 0 remaining lifespan, substract 1
    //       if it has lifespan and is alive
    //          scale its alpha channel properly
    //       if it has lifespan and its time is up
    //          destroy the entity
    for (auto& e : m_entities.getEntities())
    {
        if (e->has<CLifespan>())
        {
            int& currentLifespan = e->get<CLifespan>().remaining;
            sf::Color currentFillColor = e->get<CShape>().circle.getFillColor();
            sf::Color currentOutlineColor = e->get<CShape>().circle.getOutlineColor();
            if (currentLifespan > 0)
            {
                currentLifespan -= 1;
                e->get<CShape>().circle.setFillColor(sf::Color(currentFillColor.r, currentFillColor.g, currentFillColor.b,
                    (float)currentLifespan / (float)e->get<CLifespan>().lifespan * 255));
                e->get<CShape>().circle.setOutlineColor(sf::Color(currentOutlineColor.r, currentOutlineColor.g, currentOutlineColor.b,
                    (float)currentLifespan / (float)e->get<CLifespan>().lifespan * 255));
            }
            else
            {
                e->destroy();
            }
        }
    }
}

void Game::sCollision()
{
    auto& playerPos = player()->get<CTransform>().pos;
    auto& playerVel = player()->get<CTransform>().velocity;
    int wWidth = m_window.getSize().x;
    int wHeight = m_window.getSize().y;

    // enemies will bounce on walls, destroy the player and get killed by bullets
    for (auto& e : m_entities.getEntities("enemy"))
    {
        auto& enemyPos = e->get<CTransform>().pos;
        // bounce on walls
        if ((enemyPos.x + m_enemyConfig.CR) > wWidth || (enemyPos.x - m_enemyConfig.CR) < 0)
        {
            e->get<CTransform>().velocity.x *= -1;
        }
        else if ((enemyPos.y + m_enemyConfig.CR) > wHeight || (enemyPos.y - m_enemyConfig.CR) < 0)
        {
            e->get<CTransform>().velocity.y *= -1;
        }
        
        // collide with player
        Vec2f distFromPlayer = playerPos - enemyPos;
        if ((distFromPlayer.x * distFromPlayer.x + distFromPlayer.y * distFromPlayer.y) < 
            ((m_playerConfig.CR + m_enemyConfig.CR) * (m_playerConfig.CR + m_enemyConfig.CR)))
        {
            player()->destroy();
            for (auto& s : m_entities.getEntities("smallAlly"))
            {
                s->destroy();
            }
            m_score = 0;
            m_text.setString("Score: " + std::to_string(m_score));
            e->destroy();
            spawnPlayer();
        }

        // collide with bullets
        for (auto& b : m_entities.getEntities("bullet"))
        {
            Vec2f distFromBullet = b->get<CTransform>().pos - enemyPos;
            if ((distFromBullet.x * distFromBullet.x + distFromBullet.y * distFromBullet.y) <
                ((m_enemyConfig.CR + m_bulletConfig.CR) * (m_enemyConfig.CR + m_bulletConfig.CR)))
            {
                b->destroy();
                spawnSmallEnemies(e);
                m_score += e->get<CScore>().score;
                m_text.setString("Score: " + std::to_string(m_score));
                e->destroy();
                break;
            }
        }

        // collide with small Allies
        for (auto& s : m_entities.getEntities("smallAlly"))
        {
            Vec2f distFromSmallAlly = s->get<CTransform>().pos - enemyPos;
            if ((distFromSmallAlly.x * distFromSmallAlly.x + distFromSmallAlly.y * distFromSmallAlly.y) <
                ((m_enemyConfig.CR + m_playerConfig.CR / 2) * (m_enemyConfig.CR + m_playerConfig.CR / 2)))
            {
                s->destroy();
                spawnSmallEnemies(e);
                m_score += e->get<CScore>().score;
                m_text.setString("Score: " + std::to_string(m_score));
                e->destroy();
                break;

            }
        }
    }

    // same for small enemies except they dont bounce on walls or spawn more enemies
    for (auto& e : m_entities.getEntities("smallEnemy"))
    {
        auto& enemyPos = e->get<CTransform>().pos;
        
        // collide with player
        Vec2f distFromPlayer = playerPos - enemyPos;
        if ((distFromPlayer.x * distFromPlayer.x + distFromPlayer.y * distFromPlayer.y) <
            ((m_playerConfig.CR + m_enemyConfig.CR / 2) * (m_playerConfig.CR + m_enemyConfig.CR / 2)))
        {
            player()->destroy();
            for (auto& s : m_entities.getEntities("smallAlly"))
            {
                s->destroy();
            }
            m_score = 0;
            m_text.setString("Score: " + std::to_string(m_score));
            e->destroy();
            spawnPlayer();
        }

        // collide with bullets
        for (auto& b : m_entities.getEntities("bullet"))
        {
            Vec2f distFromBullet = b->get<CTransform>().pos - enemyPos;
            if ((distFromBullet.x * distFromBullet.x + distFromBullet.y * distFromBullet.y) <
                ((m_enemyConfig.CR / 2 + m_bulletConfig.CR) * (m_enemyConfig.CR / 2 + m_bulletConfig.CR)))
            {
                b->destroy();
                m_score += e->get<CScore>().score;
                m_text.setString("Score: " + std::to_string(m_score));
                e->destroy();
                break;
            }
        }

        // collide with small Allies
        for (auto& s : m_entities.getEntities("smallAlly"))
        {
            Vec2f distFromSmallAlly = s->get<CTransform>().pos - enemyPos;
            if ((distFromSmallAlly.x * distFromSmallAlly.x + distFromSmallAlly.y * distFromSmallAlly.y) <
                ((m_enemyConfig.CR / 2 + m_playerConfig.CR / 2) * (m_enemyConfig.CR / 2 + m_playerConfig.CR / 2)))
            {
                s->destroy();
                m_score += e->get<CScore>().score;
                m_text.setString("Score: " + std::to_string(m_score));
                e->destroy();
                break;
            }
        }
    }

    // player collide with walls
    if ((playerPos.x + m_playerConfig.CR) > wWidth || (playerPos.x - m_playerConfig.CR) < 0)
    {
        playerPos.x -= playerVel.x;
    }

    if ((playerPos.y + m_playerConfig.CR) > wHeight || (playerPos.y - m_playerConfig.CR) < 0)
    {
        playerPos.y -= playerVel.y;
    }
}

void Game::sEnemySpawner()
{
    if ((m_currentFrame - m_lastEnemySpawnTime) > m_enemyConfig.SP)
    {
        spawnEnemy();

        // record when the most recent enemy was spawned
        m_lastEnemySpawnTime = m_currentFrame;
    }
}

void Game::sSmallAllyBulletSpawner()
{
    
    if (m_random(m_randomGen) > 0.98f)
    {
        for (auto& s : m_entities.getEntities("smallAlly"))
        {
            if (m_random(m_randomGen) > 0.5f)
            {
                spawnBullet(s, Vec2f(m_xDist(m_randomGen), m_yDist(m_randomGen)));
            }
        }
    }
    
}

void Game::sCooldown()
{
    if (player()->has<CSpecial>() && !player()->get<CSpecial>().available)
    {
        if (m_currentFrame - player()->get<CSpecial>().lastfired > player()->get<CSpecial>().cooldown)
        {
            player()->get<CSpecial>().available = true;
            player()->get<CSpecial>().text.setString("Special Move Available!");
        }
    }
}

void Game::sGUI()
{
    ImGui::Begin("Geometry Wars");
    if (ImGui::BeginTabBar("MyTabBar"))
    {
        if (ImGui::BeginTabItem("Systems"))
        {
            ImGui::Checkbox("Movement", &m_movement);
            ImGui::Checkbox("Lifespan", &m_lifespan);
            ImGui::Checkbox("Cooldown", &m_cooldown);
            ImGui::Checkbox("Collision", &m_collision);
            ImGui::Checkbox("Spawning", &m_spawning);
            ImGui::SliderInt("Spawn", &m_enemyConfig.SP, 0, 120);
            if (ImGui::Button("Manual Spawn"))
            {
                spawnEnemy();
            }
            ImGui::Checkbox("Rendering", &m_render);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Entities"))
        {
            if (ImGui::CollapsingHeader("Entities by Tag"))
            {
                for (auto& [tag, entityVec] : m_entities.getEntityMap())
                {
                    if (ImGui::CollapsingHeader(tag.c_str()))
                    {
                        if (ImGui::BeginTable("", 4))
                        {
                            for (auto& e : entityVec)
                            {
                                std::string id = std::to_string(e->id());
                                Vec2f position = e->get<CTransform>().pos;
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                sf::Color eColor = e->get<CShape>().circle.getFillColor();
                                ImVec4 imguiColor(
                                    static_cast<float>(eColor.r) / 255.0f, // Red
                                    static_cast<float>(eColor.g) / 255.0f, // Green
                                    static_cast<float>(eColor.b) / 255.0f, // Blue
                                    static_cast<float>(eColor.a) / 255.0f  // Alpha
                                );

                                ImGui::PushStyleColor(ImGuiCol_Button, imguiColor);
                                if (ImGui::Button(("D##" + id).c_str()))
                                {
                                    e->destroy();
                                }
                                ImGui::PopStyleColor(1);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::TextUnformatted(id.c_str());
                                ImGui::TableSetColumnIndex(2);
                                ImGui::TextUnformatted(tag.c_str());
                                ImGui::TableSetColumnIndex(3);
                                ImGui::TextUnformatted(("(" + std::to_string((int)position.x) + "," + std::to_string((int)position.y) + ")").c_str());
                                
                            }
                            ImGui::EndTable();
                        }
                    }
                }
            }

            if (ImGui::CollapsingHeader("All Entities"))
            {                
                if (ImGui::BeginTable("", 4))
                {
                    for (auto& e : m_entities.getEntities())
                    {
                        std::string id = std::to_string(e->id());
                        Vec2f position = e->get<CTransform>().pos;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        sf::Color eColor = e->get<CShape>().circle.getFillColor();
                        ImVec4 imguiColor(
                            static_cast<float>(eColor.r) / 255.0f, // Red
                            static_cast<float>(eColor.g) / 255.0f, // Green
                            static_cast<float>(eColor.b) / 255.0f, // Blue
                            static_cast<float>(eColor.a) / 255.0f  // Alpha
                        );
                      
                        ImGui::PushStyleColor(ImGuiCol_Button, imguiColor);
                        if (ImGui::Button(("D##" + id).c_str()))
                        {
                            e->destroy();
                        }
                        ImGui::PopStyleColor(1);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(id.c_str());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(e->tag().c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextUnformatted(("(" + std::to_string((int)position.x) + "," + std::to_string((int)position.y) + ")").c_str());

                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }
    
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void Game::sRender()
{
    m_window.clear();
    if (m_render)
    {
        for (auto& e : m_entities.getEntities())
        {
            m_window.draw(e->get<CShape>().circle);
        }
    }

    m_window.draw(m_text);
    m_window.draw(player()->get<CSpecial>().text);

    // draw the ui last
    ImGui::SFML::Render(m_window);

    m_window.display();
}

void Game::sUserInput()
{
    sf::Event event;
    while (m_window.pollEvent(event))
    {
        // pass the event to imgui to be parsed
        ImGui::SFML::ProcessEvent(m_window, event);

        // this event triggers when the window is closed
        if (event.type == sf::Event::Closed)
        {
            m_running = false;
        }

        // this event is triggered when a key is pressed
        if (event.type == sf::Event::KeyPressed)
        {
            switch (event.key.code)
            {
            case sf::Keyboard::W:
                player()->get<CInput>().up = true;
                break;
            case sf::Keyboard::A:
                player()->get<CInput>().left = true;
                break;
            case sf::Keyboard::S:
                player()->get<CInput>().down = true;
                break;
            case sf::Keyboard::D:
                player()->get<CInput>().right = true;
                break;
            case sf::Keyboard::Escape:
                m_running = false;
                break;
            case sf::Keyboard::P:
                m_paused = !m_paused;
                setPaused(m_paused);
                break;

            default: break;
            }
        }

        // this event is triggered when a key is released
        if (event.type == sf::Event::KeyReleased)
        {
            switch (event.key.code)
            {
            case sf::Keyboard::W:
                player()->get<CInput>().up = false;
                break;
            case sf::Keyboard::A:
                player()->get<CInput>().left = false;
                break;
            case sf::Keyboard::S:
                player()->get<CInput>().down = false;
                break;
            case sf::Keyboard::D:
                player()->get<CInput>().right = false;
                break;
            default: break;
            }
        }

        if (event.type == sf::Event::MouseButtonPressed)
        {
            // this line ignores mouse events if ImGui is the thing being clicked
            if (ImGui::GetIO().WantCaptureMouse) { continue; }

            if (event.mouseButton.button == sf::Mouse::Left)
            {
                spawnBullet(player(), Vec2f(event.mouseButton.x, event.mouseButton.y));
            }

            if (event.mouseButton.button == sf::Mouse::Right)
            {             
                spawnSpecialWeapon(player());
            }
        }
    }
}