#include "SpaceCab.h"

using PC = Pokitto::Core;
using PD = Pokitto::Display;


//------------------------------------------------------------------------------
//  Setup 
//------------------------------------------------------------------------------

void Game::setup(GameCookie *cookie) {

	this->cookie = cookie;

    // The levels are in flash (Levels.cpp), not on an SD card: the
    // Pokitto counted SPCAB001.raw ... and imported new ones here.
    // (designer_RebuildLVLFile: the count is one past the last level.)

    this->gamePlayVariables.maxNumberOfLevels = LEVEL_COUNT;

}


//------------------------------------------------------------------------------
//  Loop
//------------------------------------------------------------------------------

void Game::loop() {

    PD::clear();
    PC::buttons.pollButtons();

    switch (this->state) {

        case GameState::SplashScreen_Init:
            this->initSplash();
            this->state = GameState::SplashScreen;
            [[fallthrough]];

        case GameState::SplashScreen:
            this->renderSplash();
            break;

        case GameState::TitleScreen_Init:
            this->titleScreen_Init();
            [[fallthrough]];

        case GameState::TitleScreen:
            this->titleScreen(this->level, this->player, this->customer);
            break;

        case GameState::PlayGame_InitGame:
            this->gamePlayVariables.levelNumber = 1;
            this->initGame(this->player);
            this->state = GameState::PlayGame_LevelIntroduction_Init;
            [[fallthrough]];

        case GameState::PlayGame_LevelIntroduction_Init:
            this->level.reset(this->gamePlayVariables.levelNumber, false);
            this->state = GameState::PlayGame_LevelIntroduction;
            [[fallthrough]];

        case GameState::PlayGame_LevelIntroduction:
            this->drawLevelStart(this->level);
            this->nextState = GameState::PlayGame_FlashingCar;
            break;

        case GameState::PlayGame_InitLevel:
            this->gamePlayVariables.thrusterFrame = 0;
            this->initLevel(this->level, this->player, this->customer, this->gamePlayVariables.levelNumber);
            this->launchCustomer(this->level, this->customer, RANDOM_START_POSITION, RANDOM_END_POSITION);
            this->state = this->nextState;
            [[fallthrough]];

        case GameState::PlayGame_FlashingCar:
        case GameState::PlayGame:
            this->inGame(this->level, this->player, this->customer);
            break;

        case GameState::MaxLevelCompleted:
            this->drawMaxLevelCompleted();
            break;

        case GameState::GameOver_Init:
            // sound.tones(GameOver_Theme);
            this->state = GameState::GameOver;
            [[fallthrough]];

        case GameState::GameOver:
            this->gameoverScreen();
            this->gamePlayVariables.levelNumber = 0;
            break;

        // case GameState::SaveScore:
        //     this->highScore.reset();
        //     this->state = GameState::HighScore;
        //     [[fallthrough]];

        case GameState::HighScore_Init:
            this->highScore_Init();
            [[fallthrough]];

        case GameState::HighScore:
            this->highScore();
            break;

        case GameState::Designer_Init:          // (no level designer: it needs the SD card)
        case GameState::Designer:
            this->state = GameState::TitleScreen_Init;
            break;

    }

    switch (state) {

        case GameState::TitleScreen:
        case GameState::PlayGame:
            if (!this->gamePlayVariables.paused) {
                this->moveCab(this->level, this->player, this->customer);
            }
            break;

        default: break;

    }

}

