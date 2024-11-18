#include "Game.h"

#include <iostream>
#include <fstream>
#include <math.h>
//#include <cstdlib>
//#include <ctime>

Game::Game(const std::string& config)
{
    init(config);
}

void Game::init(const std::string& path)
{
    // read in config file here
    std::ifstream fin("config.txt");
    std::string temp;

    while (fin >> temp)
    {
        if (temp == "Window")
        {
            int wWidth{}, wHeight{}, wFramerateLimit{}, wFullScreen{};
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
            m_text = sf::Text("Score :", m_font, fontSize);
            m_text.setFillColor(sf::Color(fontColor[0], fontColor[1], fontColor[2]));
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
    // TODO: add pause functionality in here
    //       some systems should function while paused (rendering)
    //       some systems shouldn't (movement / input)
    srand(time(0));

    while (m_running)
    {
        // update the entity manager
        m_entities.update();

        // required update call to imgui
        ImGui::SFML::Update(m_window, m_deltaClock.restart());

        sEnemySpawner();
        sMovement();
        sCollision();
        sUserInput();
        sGUI();
        sRender();

        // increment the current frame
        // may need to be moved when pause implemented
        m_currentFrame++;
    }
}

void Game::setPaused(bool paused)
{
    // TODO
}

// respawn the player in the middle of the screen
void Game::spawnPlayer()
{
    // TODO: Finish addding all properties of the player with the correct values from the config file

    // We create every entity by calling EntityManager.addEntity(tag)
    auto entity = m_entities.addEntity("player");

    // Give this entity a Transform so it spawns at (200,200) with velocity (1,1) and angle 0.0f
    entity->add<CTransform>(Vec2f(200.0f, 200.0f), Vec2f(1.0f, 1.0f), 0.0f);

    // The entity's shape will have radius 32, 8 sides, dark grey fill, and red outline of 4 points
    entity->add<CShape>(32.0f, 8, sf::Color(10, 10, 10), sf::Color(255, 0, 0), 4.0f);

    // Add an input component to the player so that we can use inputs
    entity->add<CInput>();
}

// spawn an enemy at a random position
void Game::spawnEnemy()
{
    // TODO: make sure the enemy is spawned properly with the m_enemyConfig variables
    //       the enemy must be spawned completely within the bounds of the window
    sf::Vector2u wSize = m_window.getSize();
    float spawnX = m_enemyConfig.SR + (rand() % (1 + wSize.x - 2 * m_enemyConfig.SR));
    float spawnY = m_enemyConfig.SR + (rand() % (1 + wSize.y - 2 * m_enemyConfig.SR));
    int vertices = m_enemyConfig.VMIN + (rand() % (1 + m_enemyConfig.VMAX - m_enemyConfig.VMIN));
    // speed between min and max
    float speed = m_enemyConfig.SMIN + (rand() % (int)(1 + m_enemyConfig.SMAX - m_enemyConfig.SMIN));
    // random angle between [0, 2pi] for direction
    float theta = (float)(rand()) / RAND_MAX * 2.0f * 3.141592f;
    float speedX = speed * std::cos(theta);
    float speedY = speed * std::sin(theta);

    auto entity = m_entities.addEntity("enemy");
    entity->add<CTransform>(Vec2f(spawnX, spawnY), Vec2f(speedX, speedY), 0.0f);
    entity->add<CShape>(m_enemyConfig.SR, vertices, sf::Color(255, 0, 0), sf::Color(m_enemyConfig.OR, m_enemyConfig.OG, m_enemyConfig.OB), m_enemyConfig.OT);
}

// spawns the small enemies when a big one (input entity e) explodes
void Game::spawnSmallEnemies(std::shared_ptr<Entity> e)
{
    // TODO: spawn small enemies at the location of the input enemy e

    // when we create smaller enemy, we have to read the values of the original enemy
    // - spawn a number of small enemies equal to the vertices of the original enemy
    // - set each small enemy to the same color as the original, half the size
    // - small enemies are worth double points of the original enemy
}

// spawns a bullet from a given entity to a target location
void Game::spawnBullet(std::shared_ptr<Entity> entity, const Vec2f& target)
{
    // TODO: implement the spawning of a bullet which travels toward target
    //       - bullet speed is given as a scalar speed
    //       - you must set the velocity by using formular in notes
}

void Game::spawnSpecialWeapon(std::shared_ptr<Entity> entity)
{
    // TODO: implement your own special weapon
}

void Game::sMovement()
{
    // TODO: implement all entity movement in this function
    //       you should read the m_player->Input component to determine if the player is moving

    for (auto& e : m_entities.getEntities())
    {
        auto& transform = e->get<CTransform>();
        transform.pos += transform.velocity;
    }
}

void Game::sLifespan()
{
    // TODO: implement all lifespan functionality
    //
    // for all entities
    //       if entity has no lifespan component, skip it
    //       if entity has > 0 remaining lifespan, substract 1
    //       if it has lifespan and is alive
    //          scale its alpha channel properly
    //       if it has lifespan and its time is up
    //          destroy the entity
}

void Game::sCollision()
{
    // TODO: implement all proper collisions between entities
    //       be sure to use the collision radius, NOT the shape radius
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

void Game::sGUI()
{
    ImGui::Begin("Geometry Wars");

    ImGui::Text("Stuff Goes Here");

    ImGui::End();
}

void Game::sRender()
{
    // TODO: change the code below to draw ALL of the entities
    //       sample drawing of the player Entity that we have created
    m_window.clear();

    for (auto& e : m_entities.getEntities())
    {
        e->get<CShape>().circle.setPosition(e->get<CTransform>().pos);
        e->get<CTransform>().angle += 1.0f;
        e->get<CShape>().circle.setRotation(player()->get<CTransform>().angle);

        m_window.draw(e->get<CShape>().circle);
    }

    // draw the ui last
    ImGui::SFML::Render(m_window);

    m_window.display();
}

void Game::sUserInput()
{
    // TODO: handle user input here
    //       note that you should only be setting the player's input component variables here
    //       you should not implement the player's movement logic here
    //       the movement system will read the variables you set in this function

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
                std::cout << "W Key Pressed\n";
                // TODO: set player's input component "up" to true
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
                std::cout << "W Key Released\n";
                // TODO: set player's input component "up" to false
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
                std::cout << "Left Mouse Button Clicked at (" << event.mouseButton.x << "," << event.mouseButton.y << ")" << std::endl;
                // call spawnBullet here
            }

            if (event.mouseButton.button == sf::Mouse::Right)
            {
                std::cout << "Right Mouse Button Clicked at (" << event.mouseButton.x << "," << event.mouseButton.y << ")" << std::endl;
                // call spawnSpecialWeapon here
            }
        }
    }
}