// HWSnake game for 1st Year PRAXIS course @
// Heriot Watt University, Edinburgh. Scotland
// Build your own Arduino and OLED display shield.
// Code by Will W from the EPS Electronics Workshop.
// Version 1.0
// 20th November 2021

// Dependencies are the SSD1306 library from Adafruit.
// and 'TimerInterrupt' by Khoi Hoang.
// These can be installed from 'Tools/Manage Libraries' in the menubar above.


#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>      // To save hi-score.
// Timer Interrupt for button debounce.
#define USE_TIMER_1 true
#include "TimerInterrupt.h"
#include "assert.h"

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
  #define DEBUG_PRINT_HEX(x) Serial.print(x, HEX)
  #define DEBUG_PRINTLN_HEX(x) Serial.println(x, HEX)
#else
  #define DEBUG_PRINT_FLASH(x)
  #define DEBUG_PRINTLN_FLASH(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT_HEX(x)
  #define DEBUG_PRINTLN_HEX(x)
#endif // DEBUG


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

  // The coordinates
  uint8_t y, x;

  // Overide equality operator to compare points.
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

  constexpr const uint8_t Width   { 128 };
  constexpr const uint8_t Height  { 64 };
  
  // Initialize the display.
  Adafruit_SSD1306 display( Width, Height );  
}


// The gameworld space.
namespace World {

  // How large in pixels do you want the game rows and columns to be.
  constexpr const uint8_t Scale { 6 };

  // Offsets top left and right.
  constexpr const uint8_t xMinOffset { 4 };
  constexpr const uint8_t xMaxOffset { 2 };
  constexpr const uint8_t yMinOffset { 12 };
  constexpr const uint8_t yMaxOffset { 2 };

  // How big the world is.
  constexpr const uint8_t minX  { 0 };
  constexpr const uint8_t maxX  { (Display::Width - xMinOffset - xMaxOffset) / Scale };
  constexpr const uint8_t minY  { 0 };
  constexpr const uint8_t maxY  { (Display::Height - yMinOffset - yMaxOffset) / Scale };
  
  // Where the food is.
  Point scranPos     { 0, 0 };

  // Get a random point.  This is a C++ lambda function.
  auto getRandomPoint { []() -> Point { 
    return { static_cast<uint8_t>( random(World::minY, World::maxY) ),
         static_cast<uint8_t>( random(World::minX, World::maxX) ) }; 
  }};

  // Converts game coordinates to display coordinates.
  auto toWorld { [](const Point& p) -> Point {
    return { static_cast<uint8_t>((p.y * Scale) + yMinOffset), static_cast<uint8_t>((p.x * Scale) + xMinOffset) };
  }};
}


/// Generic RingBuffer and Iterators for the snake.
template <typename T, uint8_t Size> // This is a C++ class template.  Only used functions are instantiated.
class RingBuffer {

static_assert(Size < 255, "Buffer too big.  This only takes up to 254.\n");

public:
  struct ForwardIterator;
  struct ReverseIterator;

    RingBuffer();

  // Forward iterator should iterate from the write pointer back.
  ForwardIterator begin() noexcept;
  ForwardIterator end() noexcept;

    ReverseIterator rbegin() noexcept;
  const ReverseIterator rbegin() const noexcept;
    ReverseIterator rend() noexcept;
  const ReverseIterator rend() const noexcept;

    constexpr uint8_t size() const;
  constexpr bool empty() const;
  constexpr bool full() const;
  constexpr uint8_t spaceRemaining() const;
    constexpr uint8_t capacity() const;

    T& front();
    constexpr const T& front() const;
    T& back();
    constexpr const T& back() const;

  bool push(T data);
  T pop();
  void clear();
  
private:
  T data[Size + 1]; // write == read is empty so we need 1 more index.

  // A ReverseIterator because forwards is reading from write back to read.
    ReverseIterator write;
  ReverseIterator read;
};


template <typename T, uint8_t Size>
struct RingBuffer<T, Size>::ForwardIterator {

  friend class RingBuffer<T, Size>;
    
    using self_type = RingBuffer<T, Size>::ForwardIterator;
    using value_type = T;
    using reference = T&;
  using const_reference = const T&;
    using pointer = T*;
  using const_pointer = const T*;
    using difference_type = ptrdiff_t;

    ForwardIterator(pointer ptr, RingBuffer<T, Size>& buf);
    
    reference operator*();
  constexpr const_reference operator*() const;
  pointer operator->();
  constexpr const_pointer operator->() const;
    
    // Unary operators
    // prefix
  self_type operator++();
    self_type operator--();
    // postfix
  self_type operator++(int);
    self_type operator--(int);

    // Comparison operators
    constexpr bool operator==(const self_type& other) const;
    constexpr bool operator!=(const self_type& other) const;
    constexpr bool operator<=(const self_type& other) const;
    constexpr bool operator>=(const self_type& other) const;
    
    // Arithmetic operators
  constexpr self_type operator-(const difference_type& distance) const;
  constexpr self_type operator+(const difference_type& distance) const;

    constexpr difference_type operator-(const self_type& other) const;

private:
    pointer ptr;
  RingBuffer<T, Size>& buf;
};


template <typename T, uint8_t Size>
struct RingBuffer<T, Size>::ReverseIterator {

  friend class RingBuffer<T, Size>;
    
    using self_type = RingBuffer<T, Size>::ReverseIterator;
    using value_type = T;
    using reference = T&;
  using const_reference = const T&;
    using pointer = T*;
  using const_pointer = const T*;
    using difference_type = ptrdiff_t;

    ReverseIterator(pointer ptr, RingBuffer<T, Size>& buf);
    
  reference operator*();
  constexpr const_reference operator*() const;
  pointer operator->();
  constexpr const_pointer operator->() const;
    
    // Unary operators
    // prefix
  self_type operator++();
  self_type operator--();
    // postfix
  self_type operator++(int);
  self_type operator--(int);

    // Comparison operators
  constexpr bool operator==(const self_type& other) const;
    constexpr bool operator!=(const self_type& other) const;
    constexpr bool operator<=(const self_type& other) const;
    constexpr bool operator>=(const self_type& other) const;
    
    // Arithmetic operators
  constexpr self_type operator+(const difference_type& distance) const;
  constexpr self_type operator-(const difference_type& distance) const;
  constexpr difference_type operator-(const self_type& other) const;

private:
    pointer ptr;
    RingBuffer<T, Size>& buf;
};



// The snake.
//  There is not much RAM.  If you make this too big the game might crash.
#if (DEBUG == YES)
RingBuffer<Point, 35> snake;
#else
RingBuffer<Point, 72> snake; // This is the maximum length your snake can grow to.
#endif


namespace Snake {

auto head = [](){ return snake.front(); };  // A lambda to return the head of the snake.
Direction direction { Direction::NONE };  // The direction the snake is moving.

}

// Pressing the button sets this variable.  It is volatile as it is updated from user input.
volatile Direction lastDirectionPressed { Direction::NONE };


namespace Score {

  uint16_t current { 0 };
  // Highscore is read from the EEPROM non-volatile memory.
  uint16_t high    { ((EEPROM.read(0) != 255) ? static_cast<uint16_t>(EEPROM.read(0) * 10) : 0) };
}


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

  constexpr uint8_t buttonReadTimeMillis { 2 };
  constexpr uint8_t debounceCount { 3 };
}



// ------------ Function Declarations ------------

/**
 * @brief Check if food eaten.
 * @return true if yes.
 */
inline bool detectPlayerAteScran();


/**
 * @brief Check if the player left the game area.
 * @return true if yes.
 */
inline bool detectPlayerOutOfArea(Point newHead);


/**
 * @brief Check if the player collided with himself.
 * @return false 
 */
inline bool detectSelfCollision(Point newHead);


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
 * @brief Update the score.
 */
void drawUpdatedScore();

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

    doSplashScreen();       // display the snake start up screen
  resetGameParameters();
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
          lastDirectionPressed = button->direction;
              button->state = Button::State::pressed;
          }
        }
    } else if (button->state == Button::State::pressed && (++button->unPressedCount >= debounceCount / Timing::buttonReadTimeMillis)) {

      button->state = Button::State::notPressed;
          button->pressedCount = 0;
      }
    }
}



void resetGameParameters() {

  lastDirectionPressed = Direction::NONE;
  Snake::direction = Direction::NONE;
  snake.clear();
  Score::current = 0;
  snake.push(World::getRandomPoint());
  Timing::gameUpdateTime_ms = Timing::gameUpdateTimeOnReset_ms;
  Display::display.clearDisplay();
  drawDisplayBackground();  // Draw the whole display but only once.
  placeRandomScran();
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
  display.print(Score::current);
  
  display.setCursor((Width / 2) + 2, 1);  
  display.print(F("High:"));
  display.print(Score::high);

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
  using namespace Snake;

  // Update the Snake's direction from button input.
  if (lastDirectionPressed != direction) {

    switch (lastDirectionPressed) {

      case Direction::UP:
        if (direction != Direction::DOWN)
          direction = lastDirectionPressed;
        break;
      case Direction::DOWN:
        if (direction != Direction::UP)
          direction = lastDirectionPressed;
        break;
      case Direction::LEFT:
        if (direction != Direction::RIGHT)
          direction = lastDirectionPressed;
        break;
      case Direction::RIGHT:
        if (direction != Direction::LEFT)
          direction = lastDirectionPressed;
      default: break;
    }
  }

  if (direction != Direction::NONE) {  // Nothing happens.

    // Move the Snake.
    // Save current head position.
    auto currentHead = Snake::head();
    Point newHead{};

    switch (Snake::direction) { 

      case Direction::UP:
        newHead = {static_cast<uint8_t>(currentHead.y - 1), currentHead.x };
        break;
      case Direction::DOWN:
        newHead = { static_cast<uint8_t>(currentHead.y + 1), currentHead.x };
        break;
      case Direction::LEFT:
        newHead = { currentHead.y, static_cast<uint8_t>(currentHead.x - 1) };
        break;
      case Direction::RIGHT:
        newHead = { currentHead.y, static_cast<uint8_t>(currentHead.x + 1) };
        break;
      default: break;
    }

    if (detectPlayerOutOfArea(newHead) || detectSelfCollision(newHead)) {
      doGameOver();
      return;
    } else snake.push(newHead);

    if (detectPlayerAteScran()) { // If eating tail stays put and only head advances.
      drawUpdatedScore();
    } else {
      auto removed = snake.pop();
      // best place to remove the tail.
      display.fillRect((removed.x * World::Scale) + World::xMinOffset, (removed.y * World::Scale) + World::yMinOffset, World::Scale, World::Scale, BLACK);
    }
  }
  drawSnake();
  display.display();
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
  // Only draws what is changed.
  auto& d = Display::display;
  using namespace World;
  using namespace Snake;

  auto headPos = toWorld(Snake::head());
  // draw the head.
  d.fillRect( headPos.x, headPos.y, Scale, Scale, WHITE );

  // We don't want to draw all.  We want to draw the one after the head. and the last 2.
  if (snake.size() == 1) return;
  if (snake.size() > 1) {
    auto tailPos = toWorld(snake.back());
    d.fillRect( tailPos.x, tailPos.y, Scale, Scale, BLACK);
    d.fillRect( tailPos.x + 3 , tailPos.y + 3, Scale - 3, Scale - 3, WHITE);
  }
  if (snake.size() > 2) {
    auto pos = toWorld(*(snake.end() - 2));
    d.fillRect( pos.x, pos.y, Scale, Scale, BLACK);
    d.fillRect( pos.x + 2, pos.y + 2, Scale - 2, Scale - 2, WHITE);
  }
  if (snake.size() > 3) {
    auto pos = toWorld(*(snake.begin() + 1));
    d.fillRect( pos.x, pos.y, Scale, Scale, BLACK);
    d.fillRect( pos.x + 1, pos.y + 1, Scale - 1, Scale - 1, WHITE);
  }
}

void drawUpdatedScore() {
  using namespace Display;
  // draw scores
  display.fillRect(36, 1, 27, 8, BLACK);
  display.setCursor(38, 1);
  display.print(Score::current);
}


void placeRandomScran() {

  using namespace World;

// This is a C++ lambda function.
  static auto isInSnake = [](Point& point)->bool {
    for(auto& segment : snake) {
      if (point == segment) { 
        return true; 
      };
    }
    return false;
  };

  do {
    scranPos = getRandomPoint();
    DEBUG_PRINT_FLASH("scranpos: "); DEBUG_PRINTLN(scranPos);
  } while (isInSnake(scranPos));

  // Draw scran here cause only want draw once.
  drawScran();
}


//#pragma message "detect ate scran and related game updates should be separate."
bool detectPlayerAteScran() {
    
  if (Snake::head() == World::scranPos) {

    Score::current += 10;

    if (Score::current % 100 == 0)  
      Timing::gameUpdateTime_ms -= 30;             
    
    tone(Pin::SOUND, 2000, 10);
    placeRandomScran();

    return true;
  }

  return false;
}



bool detectSelfCollision(Point newHead) {

  //Serial.print(F("Head: ")); Serial.println(*Snake::head);

  for (auto segment = snake.begin(); segment != snake.end() - 1; ++segment) {
    
    if (newHead == *segment) {

      tone(Pin::SOUND, 2000, 20);
      tone(Pin::SOUND, 1000, 20);
      DEBUG_PRINTLN_FLASH("Detected self collision.");
      return true;
    }
  }
  return false;
}



bool detectPlayerOutOfArea(Point newHead) {

#if (DEBUG == 1)
    auto rVal = (( newHead.x >= World::maxX  ) || ( newHead.y >= World::maxY ));
  if (rVal) {
    char pos[35];
    sprintf(pos, "head pos: (%u, %u)", newHead.x / World::Scale, newHead.y / World::Scale);
    DEBUG_PRINTLN(pos);
    sprintf(pos, "World min/max: (%u, %u), (%u, %u)", World::minX, World::minY, World::maxX, World::maxY);
    DEBUG_PRINTLN(pos);
    DEBUG_PRINTLN_FLASH("Detected out of area");
  }
  return rVal;
#else
  return (( newHead.x >= World::maxX ) || ( newHead.y >= World::maxY ));
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
      for (auto& segment : snake) {
        auto pos = toWorld(segment);
        display.fillRect(pos.x, pos.y, Scale, Scale, BLACK);
      }
    } else {
      auto head = toWorld(Snake::head());
      display.fillRect(head.x, head.y, Scale, Scale, WHITE);
      for (auto it = snake.begin() + 1; it != snake.end(); ++it) {
        
        auto pos = toWorld(*it);
        
        if (it == snake.end() - 2) {
          display.fillRect(pos.x + 2, pos.y + 2, Scale - 2, Scale - 2, WHITE);

        } else if (it == snake.end() - 1) {
          display.fillRect(pos.x + 3, pos.y + 3, Scale - 3, Scale - 3, WHITE);

        } else {
          display.fillRect(pos.x + 1, pos.y + 1, Scale - 1, Scale - 1, WHITE);

        }
      }
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

  if (Score::current > Score::high) {
    Score::high = Score::current;
    EEPROM.write(0, Score::high / 10);
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
  lastDirectionPressed = Direction::NONE;
  doSplashScreen();   // wait for player to start game
  resetGameParameters();
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




// *** RingBuffer ***

template<typename T, uint8_t Size>
RingBuffer<T, Size>::RingBuffer() : write{data, *this}, read{data, *this} {}

template<typename T, uint8_t Size>
typename RingBuffer<T, Size>::ForwardIterator
RingBuffer<T, Size>::begin() noexcept {
  return ForwardIterator{ &*(write - 1), *this };
}
  
template<typename T, uint8_t Size>
typename RingBuffer<T, Size>::ForwardIterator RingBuffer<T, Size>::end() noexcept {
  return ForwardIterator{ &*(read - 1), *this };
}
    
template<typename T, uint8_t Size>
typename RingBuffer<T, Size>::ReverseIterator
RingBuffer<T, Size>::rbegin() noexcept {
  return read;
}

template<typename T, uint8_t Size>
const typename RingBuffer<T, Size>::ReverseIterator
RingBuffer<T, Size>::rbegin() const noexcept {
  return read;
}

template<typename T, uint8_t Size>
typename RingBuffer<T, Size>::ReverseIterator
RingBuffer<T, Size>::rend() noexcept {
  return write; 
}

template<typename T, uint8_t Size>
const typename RingBuffer<T, Size>::ReverseIterator
RingBuffer<T, Size>::rend() const noexcept {
  return write;
}
    

template<typename T, uint8_t Size>
constexpr uint8_t RingBuffer<T, Size>::size() const {
  return ((write >= read) ? write - read : write - read + (Size + 1) );
}

template<typename T, uint8_t Size>
constexpr bool RingBuffer<T, Size>::empty() const {
    return (size() == 0);
}

template<typename T, uint8_t Size>
constexpr bool RingBuffer<T, Size>::full() const {
  return (write + 1 == read); 
}

template<typename T, uint8_t Size>
constexpr uint8_t RingBuffer<T, Size>::spaceRemaining() const {
  return capacity() - size(); 
}

template<typename T, uint8_t Size>
constexpr uint8_t RingBuffer<T, Size>::capacity() const {
  return Size; 
}

template<typename T, uint8_t Size>
T& RingBuffer<T, Size>::front() {
  assert(!empty() && "Empty");
  return *begin(); 
}

template<typename T, uint8_t Size>
constexpr const T& RingBuffer<T, Size>::front() const {
  static_assert(!empty(), "empty");
//  assert(!empty() && "Empty");
  return *begin();
}

template<typename T, uint8_t Size>
T& RingBuffer<T, Size>::back() {
  assert(!empty() && "Empty");
  return *read;  
}

template<typename T, uint8_t Size>
constexpr const T& RingBuffer<T, Size>::back() const {
  static_assert(!empty(), "empty");
//  assert(!empty() && "Empty");
  return *read; 
}

template<typename T, uint8_t Size>
bool RingBuffer<T, Size>::push(T data) {
  if (write + 1 == read)
    return false;
  *write++ = data;
  return true; 
}

template<typename T, uint8_t Size>
T RingBuffer<T, Size>::pop() {
  assert(!empty() && "Cannot pop from empty buffer");
  T rVal = *read; ++read;
  return rVal;
}

template<typename T, uint8_t Size>
void RingBuffer<T, Size>::clear() {
  write.ptr = data;
  read.ptr = data;
}


// *** Forward Iterator ***

template<typename T, uint8_t Size>
RingBuffer<T, Size>::ForwardIterator::ForwardIterator(pointer ptr, RingBuffer<T, Size>& buf) : ptr{ptr}, buf{buf} {}

template<typename T, uint8_t Size>
typename RingBuffer<T, Size>::ForwardIterator::reference
RingBuffer<T, Size>::ForwardIterator::operator*() {
  return *ptr; 
}

template<typename T, uint8_t Size>
constexpr typename RingBuffer<T, Size>::ForwardIterator::const_reference
RingBuffer<T, Size>::ForwardIterator::operator*() const { 
  return *ptr; 
}


template<typename T, uint8_t Size>
typename RingBuffer<T, Size>::ForwardIterator::pointer
RingBuffer<T, Size>::ForwardIterator::operator->() {
  return ptr;
}

template<typename T, uint8_t Size>
constexpr typename RingBuffer<T, Size>::ForwardIterator::const_pointer
RingBuffer<T, Size>::ForwardIterator::operator->() const {
  return ptr;
}


// Unary operators
// prefix
template<typename T, uint8_t Size>
typename RingBuffer<T, Size>::ForwardIterator::self_type
RingBuffer<T, Size>::ForwardIterator::operator++() {
  if (--ptr < buf.data) ptr = buf.data + Size;
  return *this;
}

template<typename T, uint8_t Size>
typename RingBuffer<T, Size>::ForwardIterator::self_type
RingBuffer<T, Size>::ForwardIterator::operator--() {
  if (++ptr == &buf.data[Size + 1]) ptr = buf.data;
  return *this;
}

// postfix
template<typename T, uint8_t Size>
typename RingBuffer<T, Size>::ForwardIterator::self_type
RingBuffer<T, Size>::ForwardIterator::operator++(int) {
  self_type rVal = *this;
  --(*this);
  return rVal; 
}

template<typename T, uint8_t Size>
typename RingBuffer<T, Size>::ForwardIterator::self_type
RingBuffer<T, Size>::ForwardIterator::operator--(int) {
  self_type rVal = *this;
  ++(*this);
  return rVal;
}


// Comparison operators
template<typename T, uint8_t Size>
constexpr bool RingBuffer<T, Size>::ForwardIterator::operator==(const self_type& other) const {
  return other.ptr == this->ptr;
}

template<typename T, uint8_t Size>
constexpr bool RingBuffer<T, Size>::ForwardIterator::operator!=(const self_type& other) const {
  return other.ptr != this->ptr;
}

template<typename T, uint8_t Size>
constexpr bool RingBuffer<T, Size>::ForwardIterator::operator<=(const self_type& other) const {
  return this->ptr >= other.ptr;
}

template<typename T, uint8_t Size>
constexpr bool RingBuffer<T, Size>::ForwardIterator::operator>=(const self_type& other) const {
  return this->ptr <= other.ptr;
}


// Arithmetic operators
template<typename T, uint8_t Size>
constexpr typename RingBuffer<T, Size>::ForwardIterator::self_type
RingBuffer<T, Size>::ForwardIterator::operator-(const difference_type& distance) const {
  return self_type(buf.data + ((ptr - buf.data + distance) % (Size + 1)), buf);
}

template<typename T, uint8_t Size>
constexpr typename RingBuffer<T, Size>::ForwardIterator::self_type
RingBuffer<T, Size>::ForwardIterator::operator+(const difference_type& distance) const {
  auto indexOfResult = ((ptr - buf.data) + ((Size + 1) - distance)) % (Size + 1);
  return self_type(buf.data + indexOfResult, buf);
}

template<typename T, uint8_t Size>
constexpr typename RingBuffer<T, Size>::ForwardIterator::difference_type
RingBuffer<T, Size>::ForwardIterator::operator-(const self_type& other) const {
  //return this->ptr - other->ptr;
  return (reinterpret_cast<intptr_t>(this->ptr) - reinterpret_cast<intptr_t>(other.ptr)) / sizeof(value_type);
}




// *** Reverse Iterator ***

template <typename T, uint8_t Size>
RingBuffer<T, Size>::ReverseIterator::ReverseIterator(pointer ptr, RingBuffer<T, Size>& buf) : ptr{ptr}, buf{buf} {}


template <typename T, uint8_t Size>
typename RingBuffer<T, Size>::ReverseIterator::reference
RingBuffer<T, Size>::ReverseIterator::operator*() {
  return *ptr;
}

template <typename T, uint8_t Size>
constexpr typename RingBuffer<T, Size>::ReverseIterator::const_reference
RingBuffer<T, Size>::ReverseIterator::operator*() const {
  return *ptr;
}

template <typename T, uint8_t Size>
typename RingBuffer<T, Size>::ReverseIterator::pointer
RingBuffer<T, Size>::ReverseIterator::operator->() {
  return ptr;
}

template <typename T, uint8_t Size>
constexpr typename RingBuffer<T, Size>::ReverseIterator::const_pointer
RingBuffer<T, Size>::ReverseIterator::operator->() const {
  return ptr;
}


// Unary operators
// prefix
template <typename T, uint8_t Size>
typename RingBuffer<T, Size>::ReverseIterator::self_type
RingBuffer<T, Size>::ReverseIterator::operator++() {
  if (++ptr == &buf.data[Size + 1]) 
    ptr = buf.data;
  return *this;
}

template <typename T, uint8_t Size>
typename RingBuffer<T, Size>::ReverseIterator::self_type
RingBuffer<T, Size>::ReverseIterator::operator--() {
  if (--ptr < buf.data)
    ptr = buf.data + Size;
  return *this;
}

// postfix
template <typename T, uint8_t Size>
typename RingBuffer<T, Size>::ReverseIterator::self_type
RingBuffer<T, Size>::ReverseIterator::operator++(int) {
  self_type rVal = *this; ++(*this);
  return rVal;
}

template <typename T, uint8_t Size>
typename RingBuffer<T, Size>::ReverseIterator::self_type
RingBuffer<T, Size>::ReverseIterator::operator--(int) {
  self_type rVal = *this;
  --(*this);
  return rVal;
}


// Comparison operators
template <typename T, uint8_t Size>
constexpr bool RingBuffer<T, Size>::ReverseIterator::operator==(const self_type& other) const {
  return this->ptr == other.ptr;
}

template <typename T, uint8_t Size>
constexpr bool RingBuffer<T, Size>::ReverseIterator::operator!=(const self_type& other) const {
  return this->ptr != other.ptr;
}

template <typename T, uint8_t Size>
constexpr bool RingBuffer<T, Size>::ReverseIterator::operator<=(const self_type& other) const {
  return this->ptr <= other.ptr;
}

template <typename T, uint8_t Size>
constexpr bool RingBuffer<T, Size>::ReverseIterator::operator>=(const self_type& other) const {
  return this->ptr >= other.ptr;
}


// Arithmetic operators
template <typename T, uint8_t Size>
constexpr typename RingBuffer<T, Size>::ReverseIterator::self_type
RingBuffer<T, Size>::ReverseIterator::operator+(const difference_type& distance) const {
  return self_type(buf.data + ((ptr - buf.data + distance) % (Size + 1)), buf);
}

template <typename T, uint8_t Size>
constexpr typename RingBuffer<T, Size>::ReverseIterator::self_type
RingBuffer<T, Size>::ReverseIterator::operator-(const difference_type& distance) const {
  auto indexOfResult = ((ptr - buf.data) + ((Size + 1) - distance)) % (Size + 1);
  return self_type(buf.data + indexOfResult, buf);
}

template <typename T, uint8_t Size>
constexpr typename RingBuffer<T, Size>::ReverseIterator::difference_type
RingBuffer<T, Size>::ReverseIterator::operator-(const self_type& other) const {
  //return this->ptr - other->ptr;
  return (reinterpret_cast<intptr_t>(this->ptr) - reinterpret_cast<intptr_t>(other.ptr)) / sizeof(value_type);
}


