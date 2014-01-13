# libretro port of Dungeon Crawl: Stone Soup
Aside from a custom version of the WindowManager and GLStateManager classes
the only change to the source was to rename the 'main' function.
(If you're wondering why I made this, it's because I want to play on my iPad)

## To Build
    git submodule update --init
    cd crawl-ref
    make -f Makefile.libretro generate
    make -f Makefile.libretro
    
## To Run
* Create an empty directory to hold game data.
* Copy crawl-ref/source/dat into the root of game directory.
* Create a file named game.crawlrc in the root of the game directory.
* Load game.crawlrc using the stonesoup_libretro core.
