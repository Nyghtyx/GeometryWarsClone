#include <SFML/Graphics.hpp>

#include "Game.h"
#include <iostream>

int main()
{
    Game g("config.txt");
    g.run();
}