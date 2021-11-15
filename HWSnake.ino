// Code by Will W from Electronics Workshop.
// Improved upon original source from ANJAWARE.
// https://www.instructables.com/Arduino-OLED-Snake-Game/
// 15th November 2021


#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>      // To save hi-score.
// Timer Interrupt for button debounce.
#define USE_TIMER_1 true
#include "TimerInterrupt.h"

// Defined for readability.
#define YES 1
#define NO 0

// This switches on or off serial debug output.
#define DEBUG NO // or NO


// If you want sound. U will need a buzzer.
#define SOUND NO
#if (SOUND == 0)
  #define tone(x,y,z)
#endif

// This makes all debugging code disappear when DEBUG set to NO.
#if (DEBUG == YES)
  #include "string.h"
  #define DEBUG_PRINT_FLASH(x) Serial.print(F(x))
  #define DEBUG_PRINTLN_FLASH(x) Serial.println(F(x))
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT_FLASH(x)
  #define DEBUG_PRINTLN_FLASH(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif // DEBUG


// All the directions you need.
enum class Direction : uint8_t {
  NONE, UP, DOWN, LEFT, RIGHT
};


// A point.
struct Point
#if (DEBUG == YES) 
: public Printable  // Inheriting from printable can allow you to print() a point.
#endif 
{
  Point() : y{0}, x{0} {}
  Point(uint8_t y, uint8_t x) : y{y}, x{x} {}
  uint8_t y, x;
  // Does this point equal another point.
  bool operator==(const Point& other) const { return (other.y == y && other.x == x); }
#if (DEBUG == YES)
  size_t printTo(Print& p) const {
    char rVal[9];
    sprintf(rVal, "(%u,%u)", x, y);
    return p.print(rVal);
  }
#endif
};


namespace Display {

  constexpr uint8_t Width   { 128 };
  constexpr uint8_t Height  { 64 };
  
  // Initialize the display.
  Adafruit_SSD1306 display( Width, Height );  
}


// The pin numbers.
namespace Pin {

  constexpr uint8_t UP  { 7 };
  constexpr uint8_t DOWN  { 8 };
  constexpr uint8_t LEFT  { 4 };
  constexpr uint8_t RIGHT { 2 };

#if (SOUND == YES)
  constexpr uint8_t SOUND { 9 };
#endif
}


// The gameworld space.
namespace World {

  // How big you want things.
  constexpr uint8_t Scale { 6 };

  // Offsets top left and right.
  constexpr uint8_t xMinOffset { 4 };
  constexpr uint8_t xMaxOffset { 2 };
  constexpr uint8_t yMinOffset { 12 };
  constexpr uint8_t yMaxOffset { 2 };

  // How big the world is.
  constexpr uint8_t minX  { 0 };
  constexpr uint8_t maxX  { (Display::Width - xMinOffset - xMaxOffset) / Scale };
  constexpr uint8_t minY  { 0 };
  constexpr uint8_t maxY  { (Display::Height - yMinOffset - yMaxOffset) / Scale };

  // Get a random point.  This is a C++ lambda function.
  auto getRandomPoint { []() -> Point { 
    return { static_cast<uint8_t>( random(World::minY, World::maxY) ),
         static_cast<uint8_t>( random(World::minX, World::maxX) ) }; 
  }};
}


// The snake.  The body value is the maximum size of the snake.
//  There is not much RAM.  If you make this too big the game might crash.
//  If you make it too small you will see what happens. 
namespace Snake {

  Direction direction { Direction::NONE };
#if (DEBUG == YES)
  Point body[35];
#else
  Point body[110];
#endif
  Point head    { 0, 0 };
  uint8_t size  { 1 };    // Snake size count. Limited to the size of the array
                //  and 255 for the size of uint8_t.
}

// Pressing the button sets this variable.
volatile Direction lastDirectionPressed { Direction::NONE };

// Food items.
bool eatingScran   { false };
Point scranPos     { 0, 0 };

uint16_t playerScore { 0 };

// Highscore is read from the EEPROM non-volatile memory.
uint16_t highScore   { ((EEPROM.read(0) != 255) ? static_cast<uint16_t>(EEPROM.read(0) * 10) : 0) };



// Define a button.
struct Button {

  enum class State { pressed, notPressed };

  constexpr Button(uint8_t pin, Direction direction) : pin{ pin }, direction{ direction } {}

// May need to be volatile..................
  mutable State state = State::notPressed;
  mutable uint8_t pressedCount { 0 };
  mutable uint8_t unPressedCount { 0 };

  const uint8_t pin;
  const Direction direction;
};



// Create buttons.
namespace Buttons {

  constexpr Button  bUp   { Pin::UP, Direction::UP },
            bDown { Pin::DOWN, Direction::DOWN },
            bLeft { Pin::LEFT, Direction::LEFT },
            bRight  { Pin::RIGHT, Direction::RIGHT };

  constexpr Button const* All[] { &bUp, &bDown, &bLeft, &bRight };
}


// Sets the pace of the game.
namespace Timing {

  uint16_t gameUpdateTime_ms { 300 };
  constexpr uint16_t gameUpdateTimeOnReset_ms { 300 };
  unsigned long lastGameUpdatedTime { 0 };

  constexpr uint8_t buttonReadTimeMillis { 3 };
  constexpr uint8_t debounceCount { 3 };

}



// ----------------- Declarations ------------------

/**
 * @brief Check if food eaten.
 * @return true if yes.
 */
inline bool detectPlayerAteScran();


/**
 * @brief Check if the player left the game area.
 * @return true if yes.
 */
inline bool detectPlayerOutOfArea();


/**
 * @brief Check if the player collided with himself.
 * @return false 
 */
inline bool detectSelfCollision();


/**
 * @brief Run the game over sequence.
 */
void doGameOver();


/**
 * @brief Draws a random Line
 * @param colour The colour to draw the line.
 */
void drawARandomLine(uint8_t colour = WHITE);


/**
 * @brief Draws the background for the game.
 */
void drawDisplayBackground();


/**
 * @brief Draw the food.
 */
void drawScran();


/**
 * @brief Draw the Snake.
 */
void drawSnake();


/**
 * @brief Place food at a random location.
 */
inline void placeRandomScran();


/**
 * @brief Read the button states and do debounce.
 */
void readButtons();


/**
 * @brief Reset snake, score and food.
 */
void resetGameParameters();


/**
 * @brief The game loop.
 */
void updateGame();


/**
 * @brief Wait for a press at the start of the game.
 */
void doSplashScreen();


#if (DEBUG == YES)
/**
 * @brief Convert directions to string.
 * @param d The direction to convert.
 * @returns String represenatation of direction.
 */
const __FlashStringHelper* directionToString(Direction d);
#endif




//  Main Functions...

void setup() {

  using namespace Display;
  delay(Timing::gameUpdateTime_ms);

  ITimer1.init();

  if (ITimer1.attachInterruptInterval(Timing::buttonReadTimeMillis, readButtons)) {
    DEBUG_PRINTLN_FLASH("Timer attached successfully.");
  }

  randomSeed(analogRead(0));
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

#if (DEBUG == 1)
  Serial.begin(9600);
#endif
    delay(Timing::gameUpdateTime_ms);
  // DEBUG_PRINT_FLASH("Size: ("); DEBUG_PRINT(World::maxX);
  // DEBUG_PRINT_FLASH(", "); DEBUG_PRINT(World::maxY);
  // DEBUG_PRINTLN_FLASH(")");

    display.clearDisplay();           // start with a clean display
    display.setTextColor(WHITE);      // set up text color rotation size etc  
    display.setRotation(0); 
    display.setTextWrap(false);
    display.dim(0);                 // set the display brighness

  for (const auto* button : Buttons::All) {
    pinMode(button->pin, INPUT_PULLUP);
  }
    
  DEBUG_PRINTLN_FLASH("Setup Complete");

  resetGameParameters();
    doSplashScreen();     // display the snake start up screen
    placeRandomScran();     // place first bit of food
}



void loop() {
  auto tNow { millis() };

  if (tNow - Timing::lastGameUpdatedTime > Timing::gameUpdateTime_ms) {
    updateGame();
    Timing::lastGameUpdatedTime = tNow;
  }
}



// This is called by the timer interrupt.
void readButtons() {
  using namespace Timing;

  for (auto& button : Buttons::All) {

      if (!digitalRead(button->pin) ) {
        button->unPressedCount = 0;

      if (++button->pressedCount >= debounceCount / Timing::buttonReadTimeMillis) {
    
            if (button->state != Button::State::pressed) {
              button->state = Button::State::pressed;
          }
        }
    } else if (button->state == Button::State::pressed && (++button->unPressedCount >= debounceCount / Timing::buttonReadTimeMillis)) {

      button->state = Button::State::notPressed;
      lastDirectionPressed = button->direction;
          button->pressedCount = 0;
      }
    }
}



void resetGameParameters() {

  lastDirectionPressed = Direction::NONE;
  Snake::direction = Direction::NONE;
  Snake::size = 1;
  Snake::head = World::getRandomPoint();
  Timing::gameUpdateTime_ms = Timing::gameUpdateTimeOnReset_ms;
  scranPos = World::getRandomPoint();
  playerScore = 0;
}



void drawDisplayBackground() {

  //DEBUG_PRINTLN_FLASH("Updating Display");
  using namespace Display;

  display.setTextSize(0);
  display.setTextColor(WHITE);
  
  display.fillRect(0, 0, Display::Width - 1, 8, BLACK);
  
  // draw scores
  display.setCursor(2, 1);
  display.print(F("Score:"));
  display.print(playerScore);
  
  display.setCursor((Width / 2) + 2, 1);  
  display.print(F("High:"));
  display.print(highScore);

  // draw play area
  //      pos  1x, 1y, 2x, 2y, colour
  display.drawLine(0, 0, Width - 1, 0, WHITE);            // very top border
  display.drawLine((Width / 2) - 1, 0, (Width / 2) - 1, 9, WHITE);  // score seperator
  display.fillRect(0, 9, Width - 1, 2, WHITE);            // below text border
  display.drawLine(0, 0, 0, 9, WHITE);
  display.drawLine(Width - 1, 0, Width - 1, 9, WHITE); 

  display.fillRect(0, Height - 3, Width - 1, 3, WHITE);       // bottom border
  display.fillRect(0, 9, 3, Height - 1, WHITE);             // left border
  display.fillRect(Width - 3, 9, 3, Height - 1, WHITE);         // right border    
}



void updateGame() {
    
  using namespace Display;
  
  #pragma message "Inefficient use pointer to head and tail."
  // Move the body. 
  for (uint8_t i { Snake::size }; i > 0; --i) {
    Snake::body[i] = Snake::body[i - 1];
  }

  eatingScran = detectPlayerAteScran();

  // Add an extra pixel to the snake.
  if (eatingScran) {
    ++Snake::size;
    Snake::body[Snake::size - 1] = Snake::head;
  }

  Snake::body[0] = Snake::head;
  
  if (lastDirectionPressed != Snake::direction) {

    switch (lastDirectionPressed) {
      case Direction::UP:
        if (Snake::direction != Direction::DOWN)
          Snake::direction = lastDirectionPressed;
        break;
      case Direction::DOWN:
        if (Snake::direction != Direction::UP)
          Snake::direction = lastDirectionPressed;
        break;
      case Direction::LEFT:
        if (Snake::direction != Direction::RIGHT)
          Snake::direction = lastDirectionPressed;
        break;
      case Direction::RIGHT:
        if (Snake::direction != Direction::LEFT)
          Snake::direction = lastDirectionPressed;
        break;
      default: break;
    }
  }

  // Move the Snake.
  switch(Snake::direction) { 

    case Direction::UP:
      --Snake::head.y;
      break;
    case Direction::DOWN:
      ++Snake::head.y;
      break;
    case Direction::LEFT:
      --Snake::head.x;
      break;
    case Direction::RIGHT:
      ++Snake::head.x;
      break;
    default: break;
  }

  #pragma message "Redrawing everything is inefficient.  Should rewrite."

  display.clearDisplay();
  drawDisplayBackground();
  drawScran();
  drawSnake();
  display.display();

  if (detectPlayerOutOfArea() || detectSelfCollision())
    doGameOver();
}



void doSplashScreen() {

  using namespace Display;
  display.clearDisplay();

  while (true) {

    auto tNow { millis() };

    if (tNow - Timing::lastGameUpdatedTime > Timing::gameUpdateTime_ms) {

      drawARandomLine(); // draw a random white line
      drawARandomLine(BLACK); // draw a random black line so that the screen not completely fill white

      display.fillRect(19, 20, 90, 32, BLACK); // blank background for text
      display.setTextColor(WHITE);
      display.setCursor(35, 25);
      display.setTextSize(2); // bigger font
      display.println(F("SNAKE"));
              //    x  y   w  h r  col
      display.drawRoundRect(33, 22, 62, 20, 4,WHITE);  // border Snake
      display.drawRect(19, 20, 90, 32, WHITE);         // border box  - 3
      display.setCursor(28, 42);
      display.setTextSize(0);  // font back to normal

      display.println(F("press any key"));
      display.display();
      
      Timing::lastGameUpdatedTime = tNow;
    }

    if (lastDirectionPressed != Direction::NONE) {
      lastDirectionPressed = Direction::NONE;
      break;
    }
  }
}



void drawARandomLine(uint8_t colour) {

  auto getRand { [](uint8_t max) -> uint8_t { return static_cast<uint8_t>(random(0, max)); } };

  Point start { getRand(Display::Height), getRand(Display::Width) };
  Point end {   getRand(Display::Height), getRand(Display::Width) };
  
  Display::display.drawLine(start.x, start.y, end.x, end.y, colour);
}


void drawScran() {

  auto& d = Display::display;
  using namespace World;
  
  d.drawRect ( ( scranPos.x * Scale ) + xMinOffset,
         ( scranPos.y * Scale ) + yMinOffset,
           Scale,
           Scale,
           WHITE
        );
}


void drawSnake() {

  auto& d = Display::display;
  using namespace World;
  using namespace Snake;

  d.fillRect( ( body[0].x * Scale ) + xMinOffset,
        ( body[0].y * Scale ) + yMinOffset,
          Scale,
          Scale,
          WHITE
        );

  for (uint16_t i { 1 }; i < static_cast<uint16_t>(Snake::size - 1); ++i) {
    
    d.fillRect( ( body[i].x * Scale ) + 1 + xMinOffset,
          ( body[i].y * Scale ) + 1 + yMinOffset,
            Scale - 1,
            Scale - 1,
            WHITE  
          );
  }

  d.fillRect( ( body[size - 1].x * Scale) + 2 + xMinOffset,
        ( body[size - 1].y * Scale) + 2 + yMinOffset,
          Scale - 2,
          Scale - 2,
          WHITE  
        );
}


void placeRandomScran() {

// This is a C++ lambda function.
  static auto isInSnake = [](Point& point)->bool {
    for(int i = 0; i < Snake::size; ++i) {
      if (point == Snake::body[i]) { 
        DEBUG_PRINTLN_FLASH("In snake retry.");
        return true; 
      };
    }
    return false;
  };

  do {
    scranPos = World::getRandomPoint();
    DEBUG_PRINT_FLASH("scranpos: "); DEBUG_PRINTLN(scranPos);
  } while (isInSnake(scranPos));
}



bool detectPlayerAteScran() {
    
  if(Snake::head.x == scranPos.x && Snake::head.y == scranPos.y) {
    playerScore += 10;
    if (playerScore % 100 == 0) { Timing::gameUpdateTime_ms -= 35; }             
    tone(Pin::SOUND, 2000, 10);
    drawDisplayBackground();
    placeRandomScran();
    return true;
  }
  return false;
}



bool detectSelfCollision() {

  for (uint16_t i = 4; i < Snake::size; i++) {
    if (Snake::head.x == Snake::body[i].x && Snake::head.y == Snake::body[i].y) {

      tone(Pin::SOUND, 2000, 20);
      tone(Pin::SOUND, 1000, 20);
      DEBUG_PRINTLN_FLASH("Detected self collision.");
      return true;
    }
  }
  return false;
}



bool detectPlayerOutOfArea() {

#if (DEBUG == 1)
    auto rVal = (( Snake::head.x >= World::maxX  ) || ( Snake::head.y >= World::maxY ));
  if (rVal) {
    char pos[35];
    sprintf(pos, "head pos: (%u, %u)", Snake::head.x / World::Scale, Snake::head.y / World::Scale);
    DEBUG_PRINTLN(pos);
    sprintf(pos, "World min/max: (%u, %u), (%u, %u)", World::minX, World::minY, World::maxX, World::maxY);
    DEBUG_PRINTLN(pos);
    DEBUG_PRINTLN_FLASH("Detected out of area");
  }
  return rVal;
#else
  return (( Snake::head.x >= World::maxX ) || ( Snake::head.y >= World::maxY ));
#endif
}



void doGameOver() {
    
  using namespace Display;
  using namespace World;
  
  // Flash the snake
  bool on = false;
  uint8_t dly = 60;

  for (uint8_t i = 0; i < 17; ++i) {
    if (!on) {
      for (uint8_t i = 0; i < Snake::size; ++i) {
        const auto& segment = Snake::body[i];
        display.fillRect( ( segment.x * Scale ) + xMinOffset,
                  ( segment.y * Scale ) + yMinOffset,
                  Scale,
                  Scale,
                  BLACK
        );
      }
    } else {
      drawSnake();
    }
    display.display();
    on = !on;
    delay(dly);
    dly -= 4;
  }
  delay(350);

  uint8_t rectX1 { 38 }, rectY1 { 28 }, rectX2 { 58 }, rectY2 { 12 };
    
  display.clearDisplay();
    display.setCursor(40, 30);
    display.setTextSize(1);
    
  tone(Pin::SOUND, 2000, 50);
    display.print(F("GAME "));
    delay(500);
  tone(Pin::SOUND, 1000, 50);
    display.print(F("OVER"));

  if (playerScore > highScore) {
    highScore = playerScore;
    EEPROM.write(0, highScore / 10);
  }

    for (uint8_t i = 0; i <= 16; ++i) { // this is to draw rectangles around game over

    display.drawRect(rectX1, rectY1, rectX2, rectY2, WHITE);
    display.display();

    rectX1 -= 2;      // shift over by 2 pixels
    rectY1 -= 2;
    rectX2 += 4;      // shift over 2 pixels from last point
    rectY2 += 4;

    tone(Pin::SOUND, i * 200, 3);
  }
    
  display.display();          

  rectX1 = 0;   // set start position of line
  rectY1 = 0;
  rectX2 = 0;
  rectY2 = 63;

  for (uint8_t i = 0; i <= 127; i++) {
    
    display.drawLine(rectX1, rectY1, rectX2, rectY2, BLACK); 
    ++rectX1;
    ++rectX2;
    display.display();                          
    }
    
  display.clearDisplay();

  resetGameParameters();
  doSplashScreen();   // wait for player to start game    
}



#if (DEBUG == 1)
const __FlashStringHelper* directionToString(Direction d) {

  switch (d) {
    case Direction::UP:
      return F("Up");
      break;
    case Direction::DOWN:
      return F("Down");
      break;
    case Direction::LEFT:
      return F("Left");
      break;
    case Direction::RIGHT:
      return F("Right");
      break;
    case Direction::NONE:
      return F("None");
      break;
    default: return F("error");
  }
}
#endif
