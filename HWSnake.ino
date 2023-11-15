// HWSnake game for 1st Year PRAXIS course @
// Heriot Watt University, Edinburgh. Scotland
// Build your own Arduino and OLED display shield.
// Coded by Will W from the EPS Electronics Workshop with too much abstraction
// and too much C++, mostly for his own entertainment.
// Version 2.0 was created in April 2023
// The snake should now grow to fill the entire screen if you are good enough.
// Previous version was Version 1.0 on the 20th November 2021

// Dependencies are the SSD1306 library from Adafruit.
// and 'TimerInterrupt' by Khoi Hoang.
// These can be installed from 'Tools/Manage Libraries' in the menubar above if
// in the Arduino IDE or from the Platformio library manager if using vscode.

// All files combined into one for ease of use for students.


#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>			// To save hi-score.
// Timer Interrupt for button debounce.
#define USE_TIMER_1 true
#include "TimerInterrupt.h"


// Defined for readability.
#define YES 1
#define NO 0

// This switches on or off serial debug output.
#define DEBUG NO // or NO
#define LIVE_ERRORS NO

// Write yes to remove the high score.
#define CLEAR_HIGH_SCORE NO

// This determines the size of the array used to store the snake.  The
// snake may be 4 times this size + 1 for the head.  Maximum if all spaces 
// were in the snake would be 160 sections so 40 bytes of data should be enough.
#if (DEBUG == YES)
constexpr uint8_t SNAKE_DATA_SIZE { 4 };
#elif (DEBUG == NO)
constexpr uint8_t SNAKE_DATA_SIZE { 40 };
#endif


// If you want sound. U will need a buzzer.
#define SOUND NO
#if (SOUND == 0)
	#define tone(x, y, z)
#endif


// This makes all debugging print statements disappear when DEBUG set to NO.
#if (DEBUG == YES)
	#include "string.h"
	#define DEBUG_PRINT_FLASH(x) 	Serial.print(F(x))
	#define DEBUG_PRINTLN_FLASH(x) 	Serial.println(F(x))
	#define DEBUG_PRINT(x) 			Serial.print(x)
	#define DEBUG_PRINTLN(x) 		Serial.println(x)
	#define DEBUG_PRINT_HEX(x) 		Serial.print(x, HEX)
	#define DEBUG_PRINTLN_HEX(x) 	Serial.println(x, HEX)
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

	constexpr uint8_t UP 	{ 7 };
	constexpr uint8_t DOWN 	{ 8 };
	constexpr uint8_t LEFT 	{ 4 };
	constexpr uint8_t RIGHT { 2 };
	constexpr uint8_t MIDDLE{ 3 };

#if (SOUND == YES)
	constexpr uint8_t SOUND { 9 };
#endif
}



// Geometry Header
// Header only library containing definition of Point and it's alias Size.
// 	as well as Rectangle.

// A point
// Templated to allow for changing the stored data type easily throughout the whole
//	program.
template <typename T>
struct Point
#if (DEBUG == YES) 
: public Printable	// Inheriting from printable can allow you to print() a point.
#endif 
{
	// The Coordinates
	T y, x;

  // Constructors.
	Point() : y{0}, x{0} {}
	Point(T y, T x) : y{y}, x{x} {}

	template <typename OtherPointDataType>
	Point(const Point<OtherPointDataType>& other) : y{other.y}, x{other.x}  {}


	// Override equality operator to compare points.
	bool operator==(const Point& other) const { return (other.y == y && other.x == x); }
	bool operator!=(const Point& other) const { return !(*this == other); }
	// Override addition assignment operator to add and assign.
	void operator+=(const Point& other) { y += other.y; x += other.x; }
	void operator-=(const Point& other) { y -= other.y; x -= other.x; }

#if (DEBUG == YES)
size_t printTo(Print& p) const {
	size_t cnt { 0 };
	cnt += p.print('(');
	cnt += p.print(y);
	cnt += p.print(", ");
	cnt += p.print(x);
	cnt += p.print(')');
	return cnt;
}
#endif
};


//template <typename T> using Size = Point<T>;

template <typename PointT, typename DataT = decltype(PointT::x)>
struct Rectangle 
#if (DEBUG == YES)
: public Printable
#endif
{
	using SizeT = PointT;
private:
// Origin is tl
	PointT mOrigin;
	SizeT mSize;
public:
	Rectangle() : mOrigin{PointT{0,0}}, mSize{PointT{0,0}} {}
	Rectangle(DataT y, DataT x, DataT height, DataT width) : mOrigin{y, x}, mSize{height, width} {}
	Rectangle(const PointT& tl, const PointT& tr, const PointT& bl, const PointT& br) : mOrigin{ tl.y, tl.x }, mSize{ bl.y - tl.y, tr.x - tl.x } {} 
	Rectangle(const PointT& origin, const SizeT& size) : mOrigin{origin}, mSize{size} {}
	Rectangle(const SizeT& size) : mOrigin{0, 0}, mSize{ size } {}

	template <typename OtherPointType>
	Rectangle(const Rectangle<OtherPointType>& other) {
		this->mOrigin = other.origin();
		this->mSize = other.size();
	}

	PointT tl() const { return mOrigin; }
	PointT bl() const { return { mOrigin.y + mSize.y, mOrigin.x }; }
	PointT tr() const { return { mOrigin.y, mOrigin.x + mSize.x }; }
	PointT br() const { return { static_cast<DataT>(mOrigin.y + mSize.y), static_cast<DataT>(mOrigin.x + mSize.x) }; }
	PointT origin() const { return mOrigin; }
	PointT centre() const { 
		return { 	static_cast<DataT>(mOrigin.y + (mSize.y / 2)),
		 			static_cast<DataT>(mOrigin.x + (mSize.x / 2)) 
		};
	}

	constexpr SizeT size() const { 	return mSize; }
	constexpr DataT height() const { return mSize.y; }
	constexpr DataT width() const { return mSize.x; }
	constexpr DataT minY() const { return mOrigin.x; }
	constexpr DataT minX() const { return mOrigin.y; }
	constexpr DataT maxY() const { return br().y; }
	constexpr DataT maxX() const { return br().x; }

	Rectangle& grow(DataT amountY, DataT amountX) { 
			mOrigin.y -= amountY;
			mOrigin.x -= amountX;
			mSize.y += (amountY + amountY);
			mSize.x += (amountX + amountX);
			return *this;
	}

	Rectangle& grow(DataT amount) { return grow(amount, amount); }
	

	template <typename OtherPointDataType>
	Rectangle& centreOn(const Point<OtherPointDataType>& point) { 
			
		mOrigin.x = point.x - (mSize.x / 2);
		mOrigin.y = point.y - (mSize.y / 2);
		return *this;
	}
	
	template <typename OtherPointType>
	Rectangle& centreOn(const Rectangle<OtherPointType>& other) { 
		return centreOn(other.centre()); 
	}


#if (DEBUG == YES)
	size_t printTo(Print& p) const {
		size_t r {0};
		r += p.print("origin: ");
		r += p.print(mOrigin);
		r += p.print(", size: ");
		r += p.print(mSize);
		return r;
	}
#endif

};


namespace Utility {

// A utility to tell if an integer type is signed or unsigned at runtime or compile time.
template <typename T>
struct is_signed {
	static constexpr bool value { (T(-1) < T(0)) }; 
};

template <typename T>
struct is_unsigned {
	static constexpr bool value { (T(-1) > T(0)) };
};

}



// Store Points as a pair of this type.
// int8_t will give a range of -127 to +128.
// uint8_t will give a range of 0 to 255.
// The data type used throughout.
using POINT_DATA_TYPE = uint8_t;
using PointType = Point<POINT_DATA_TYPE>;
using SizeType = PointType;
using Rect = Rectangle<PointType>;





namespace Display {
	constexpr uint8_t Address { 0x3C };
	const Rect dspRect { {64, 128} };
}


namespace World {

	// How large in pixels do you want the game rows and columns to be.
	constexpr uint8_t Scale { 6 };

	// Offsets top left and right.
	constexpr uint8_t xMinOffset { 4 };
	constexpr uint8_t xMaxOffset { 2 };
	constexpr uint8_t yMinOffset { 12 };
	constexpr uint8_t yMaxOffset { 2 };

	// How big the world is.
	Rect World {
			0,																	// minY
			0,																	// minX
			(Display::dspRect.height() - yMinOffset - yMaxOffset) / Scale,		// maxY
			(Display::dspRect.width() - xMinOffset - xMaxOffset) / Scale 		// maxX
	};
}


// All the directions you need.
// First 4 directions are reversible if bits are reversed.
// Middle is for middle button.
// ie UP is 0b00 and down is 0b11.
enum class Direction : uint8_t {
    UP = 0, LEFT, RIGHT, DOWN, NONE, MIDDLE
};

// Flipping the bits reverses the Direction.
Direction operator~(const Direction direction);

// #if (DEBUG == YES)
// String DirectionAsString(const Direction d);
// #endif // (DEBUG == YES)



// namespace Error {

// Adafruit_SSD1306* displayPtr = nullptr;

// void initErrors(Adafruit_SSD1306& display) {
// 	displayPtr = &display;
// }

// void displayError(int16_t line, const char* file, const char* msg) {

// 	if (displayPtr == nullptr) return;
	
// 	auto& d = *displayPtr;

// 	d.clearDisplay();
// 	d.setTextColor(WHITE);
// 	d.setCursor(0, 0);
// 	d.setTextSize(1);
// 	d.println("ERROR");
// 	d.println("Line: ");
// 	d.println(line);
// 	d.println("File: ");
// 	d.println(file);
// 	d.println(msg);
// 	d.display();
// 	delay(10000);
// }
// }


// Optional wrapper for a point. ie it may or may not contain data.
template <typename DATA_TYPE>
struct OptionalPoint : protected Point<DATA_TYPE> {
private:
	const bool mHasValue { true };
public:
	OptionalPoint(const DATA_TYPE y, const DATA_TYPE x) :Point<DATA_TYPE>{y, x} { }
	OptionalPoint(const Point<DATA_TYPE>& p) : Point<DATA_TYPE>{p} { }
	OptionalPoint() : mHasValue{ false } { }
	bool hasValue() const { return hasValue; }
	bool isEmpty() const { return !hasValue; }
	const Point<DATA_TYPE> getValue() const { return {this->y, this->x}; }
	operator bool() { return mHasValue; }
};



// Template function definitions are defined in the header.

namespace Utility {

//#if (DEBUG == YES)
// template<typename T>
// size_t Binary<T>::printTo(Print& p) const {

// 	char binStr[sizeof(T) + 2];
// 	T b { this->value };
// 	T rev {0};

//     for (uint16_t i{}; i < static_cast<uint16_t>(sizeof(T)) * 8; ++i) {
//         rev = rev << 1;
//         rev += (b % 2);
//         b = b >> 1;
//     }
//     for (uint16_t i{}; i < static_cast<uint16_t>(sizeof(T)) * 8; ++i) {
//     	binStr[i] = (rev % 2);
//         rev = rev >> 1;
//     }
// 	return p.print(binStr);
// }
// #endif // (DEBUG == YES)
}


// A crumb is half of a nibble which is half of a byte so it is essentially 2-bits.
//  The smallest amount of memory addresable by a pointer in C++ is a byte so this struct
//  is required to address a smaller amount.  Allowing the Snake to be stored in 1/8 of the 
//  memory it would be if one were to store 2-byte points (uint8_t, uint8_t).  Instead we store
//  the coordinate of the Snake's tail and then iterate through it's body by using directions.
//  Up, Down, Left and Right.  These 4 can be stored in a crumb or 2 bits.  00, 01, 10, and 11.
//  On an 8-bit avr pointers are stored as bytes.  I could have used a 16-bit data type and
//  shifted in and out the crumb using left and right shift to separate the pointer from the 
//  crumb value, however i developed the snake on a 64-bit pc.  The largest integer data type 
//  in C++ is 64-bit so instead i created this struct.
struct CrumbPtr 
#if (DEBUG == YES)
: public Printable
#endif // (DEBUG == YES)
{
    
	uint8_t* ptr; // 64 bits on a pc. 8 bits on an avr.  
	uint8_t crumb;
  
	using selfType = CrumbPtr;

	CrumbPtr(uint8_t* ptr, uint8_t crumb) : ptr{ptr}, crumb{crumb} {}
	Direction getValue();
  	void putValue(Direction newval);
	
	bool operator==(const CrumbPtr&) const;
	bool operator!=(const CrumbPtr&) const;
	selfType& operator++(); // Prefix
	selfType operator++(int); // Postfix
	selfType& operator--(); // Prefix
	selfType operator--(int); // Postfix

#if (DEBUG == YES)
	size_t printTo(Print& p) const;
#endif
};


//    ---- memory ----
// <  ================  <0>
//  memstart       memend
template <uint8_t SNAKE_DATA_SIZE, typename POINT_TYPE>
class Snake 
#if (DEBUG == YES)
: public Printable
#endif 
{

	uint8_t data[SNAKE_DATA_SIZE] {};
	uint16_t m_length { 0 };
	Direction m_dir { Direction::NONE };
	POINT_TYPE m_head {};
	POINT_TYPE m_tail {};
	CrumbPtr memstart { data, 0 }; // When popping memstart is increased.
	CrumbPtr memend { data, 0 }; // When pushing memend is increased.
    
public:
    
	//Snake() : data{}, m_length{}, m_dir{Direction::NONE}, m_head{}, m_tail{}, memstart{data, 0}, memend {data, 0}  {}
    uint16_t capacity() const { return 1 + (sizeof(data) * 4); }
    bool full() const { return ( m_length == capacity() ); }
    bool empty() const { return ( m_length == 0 ); }
    uint16_t length() const { return m_length; }
    const POINT_TYPE& head() const { return m_head; } 
    const POINT_TYPE& tail() const { return m_tail; }
	Direction getDirection() const { return m_dir; }
	void setDirection(Direction d) { m_dir = d; }

    // Pushing adds to the head end.
    bool push(const POINT_TYPE& p);
    // pop
    const POINT_TYPE pop();    
	// Access index using subscript operator.
	const POINT_TYPE operator[](size_t index) const;
	// Point is in the snake. and return detected point.
	OptionalPoint<POINT_DATA_TYPE> pointIsInside(const POINT_TYPE& p);

#if (DEBUG == YES)
	size_t printTo(Print& p) const;
#endif
};


template <uint8_t SNAKE_DATA_SIZE, typename POINT_TYPE>
bool Snake<SNAKE_DATA_SIZE, POINT_TYPE>::push(const POINT_TYPE& p) {
    
	//DEBUG_PRINT("Len: ");
	//DEBUG_PRINTLN(m_length);
	if (full()) { return false; } // Basically if full don't add more.

	if (m_length > 0) {  
        // move current head to memory.
        if (p.y == m_head.y + 1) {		memend.putValue(Direction::DOWN); }
        else if (p.x == m_head.x + 1) { memend.putValue(Direction::LEFT); }
        else if (p.y + 1 == m_head.y) { memend.putValue(Direction::UP); }
        else if (p.x + 1 == m_head.x) { memend.putValue(Direction::RIGHT); }
        else {
			DEBUG_PRINTLN_FLASH("Error. Bad insert.\n");
			//exit(1);
		}
        
        ++memend;

        if (memend.ptr > data + sizeof(data) - 1)
            memend.ptr = data;
        // Round off the ptr here.
        // add new point to head.
	//	DEBUG_PRINT_FLASH("Setting head to: ");
	//	DEBUG_PRINTLN(p);
        m_head = p;
        
    } // insert dir in mem
    else { m_head = m_tail = p; } // If empty.
    m_length++;
    return true;
}

template <uint8_t SNAKE_DATA_SIZE, typename POINT_TYPE>
const POINT_TYPE Snake<SNAKE_DATA_SIZE, POINT_TYPE>::pop() {

    if (empty()) { return POINT_TYPE { 0, 0 }; } // Should never be empty in the game.

    DEBUG_PRINT_FLASH("Len: ");
	DEBUG_PRINTLN(m_length);
	const POINT_TYPE rval = m_tail;
	if (m_length == 1) { 
		m_tail = m_head = { 0, 0 };
		m_length = 0;
		return rval;
	}// Length of 1 head and tail are reset and snake length set to zero.
	if (m_length == 2) {
		m_tail = m_head;
		m_length--;
		memstart.ptr = memend.ptr = data;	// Reset the memory.
		memstart.crumb = memend.crumb = 0;
		return rval; 
	} // Length of 2 so tail set to head and no need to adjust the buffer.
    DEBUG_PRINTLN("p");
    switch(~memstart.getValue()) {
        
        case Direction::UP: m_tail += { 1, 0 }; break;
        case Direction::DOWN: m_tail -= { 1, 0 }; break;
        case Direction::LEFT: m_tail -= { 0, 1 }; break;
        case Direction::RIGHT: m_tail += { 0, 1 }; break;
            
        default: exit(1);
    }
//	DEBUG_PRINT_FLASH("m_tail set to: ");
//	DEBUG_PRINTLN(m_tail);
    memstart++;
    if (memstart.ptr > data + sizeof(data) - 1)
		memstart.ptr = data;
	
//	DEBUG_PRINTLN_FLASH("m_length--");
    m_length--;
	DEBUG_PRINTLN("r");
    return rval;
}

#pragma message "Would be more efficient if index was counted from closest end of snake."
template <uint8_t SNAKE_DATA_SIZE, typename POINT_TYPE>
const POINT_TYPE Snake<SNAKE_DATA_SIZE, POINT_TYPE>::operator[](size_t index) const {
	// we are talking head to tail index here.
	// tail to head is counting forward and head to tail is counting backwards.
	
	// Print an error.
	if (index + 1 > m_length) { 
#pragma warning "uncomment"		//Error::displayError(__LINE__, __FILE__, "Out of range access.");
		return {0, 0}; 
	}
	if (index == 0) { return m_head; };
	
	// So we follow the snake from the tail.
	int indexFromTail { static_cast<int>(m_length) - static_cast<int>(index) - 1 };
	CrumbPtr cp { memstart };
	POINT_TYPE p { m_tail };
	
	for (int i{0}; i < indexFromTail; ++i) {
		
		switch(~cp.getValue()) {
	
			case Direction::UP: p += { 1, 0 }; break;
			case Direction::DOWN: p -= { 1, 0 }; break;
			case Direction::LEFT: p -= { 0, 1 }; break;
			case Direction::RIGHT: p += { 0, 1 }; break;
			default: exit(1);
		}
		cp++;
		// I don't agree with the constness here.  Data is never altered.
		if (cp.ptr > data + sizeof(data) - 1) cp.ptr = const_cast<uint8_t*>(data);
	}
	return p;
}


template <uint8_t SNAKE_DATA_SIZE, typename POINT_TYPE>
OptionalPoint<POINT_DATA_TYPE> Snake<SNAKE_DATA_SIZE, POINT_TYPE>::pointIsInside(const POINT_TYPE& p) {
	
	if (p == m_head) return { m_head };
	if (m_length == 1) return OptionalPoint<POINT_DATA_TYPE>();
	
	//DEBUG_PRINT_FLASH("p: ");
	//DEBUG_PRINTLN(p);
	
	auto segment {m_head};
	auto cp { memend };
	--cp;
	auto end { memstart };
	// deal with the below afterwards by returning the colliding point and discarding
	// if it is the tail.
	//++end; // We want to end just before tail because tail will move out of the way.
	//if (end.ptr > data + sizeof(data) - 1) end.ptr = data;

//	DEBUG_PRINT_FLASH("memstart: "); DEBUG_PRINTLN(memstart);
//	DEBUG_PRINT_FLASH("memend: "); DEBUG_PRINTLN(memend);
//	DEBUG_PRINT(m_head);

	for (;; cp--) {

		if (cp.ptr < data) cp.ptr = data + (sizeof(data) - 1);

//		DEBUG_PRINT_FLASH("cp: "); DEBUG_PRINTLN(cp);
		
		switch(cp.getValue()) {
            
             case Direction::UP: 	segment += { 1, 0 }; break;
             case Direction::DOWN: 	segment -= { 1, 0 }; break;
             case Direction::LEFT: 	segment -= { 0, 1 }; break;
             case Direction::RIGHT: segment += { 0, 1 }; break;
             default: exit(1);
        }
//		DEBUG_PRINT(segment);
		if (segment == p) return { segment };
		if (cp == end) break;
	}
	//DEBUG_PRINTLN();
	return OptionalPoint<POINT_DATA_TYPE>{};
}


#if (DEBUG == YES)
template <uint8_t SNAKE_DATA_SIZE, typename POINT_TYPE>
size_t Snake<SNAKE_DATA_SIZE, POINT_TYPE>::printTo(Print& p) const {
	
	size_t count {0};
	POINT_TYPE pnt {m_tail};
	auto it { memstart };

	DEBUG_PRINT_FLASH("<<<");
	count += p.print(m_tail);

	for (uint16_t i = 0; i < m_length - 1; ++i) {

		switch(~it.getValue()) {
            
             case Direction::UP: 	pnt += { 1, 0 }; break;
             case Direction::DOWN: 	pnt -= { 1, 0 }; break;
             case Direction::LEFT: 	pnt -= { 0, 1 }; break;
             case Direction::RIGHT: pnt += { 0, 1 }; break;
             default: exit(1);
        }
		
		count += p.print(pnt);
		it++;

		if (it.ptr > data + sizeof(data) - 1) it.ptr = const_cast<uint8_t*>(data); 
	}
	
	DEBUG_PRINT_FLASH(":=<");
	return count;
}
#endif












using SnakeType = Snake<SNAKE_DATA_SIZE, PointType>;

// This is our snake.
SnakeType snake {};

// Pressing a button sets this variable.  It is volatile as it is updated from user input.
volatile Direction lastDirectionPressed { Direction::NONE };


namespace Display {
	// Initialize the display.
	Adafruit_SSD1306 display( dspRect.width(), dspRect.height() );  
}


// The gameworld space.
namespace World {
	
	// Where the food is.
	PointType scranPos {};

	// Get a random point.  This is a C++ lambda function.
	auto getRandomPoint { []()->PointType {

		return { static_cast<POINT_DATA_TYPE>( random(World.minY(), World.maxY()) ),
				 static_cast<POINT_DATA_TYPE>( random(World.minX(), World.maxX()) ) }; 
	}};

	// Converts game coordinates to display coordinates.
	auto toWorld { [](const PointType& p)->PointType {
		return {	static_cast<POINT_DATA_TYPE>((p.y * Scale) + yMinOffset),
					static_cast<POINT_DATA_TYPE>((p.x * Scale) + xMinOffset) };
	}};
}


namespace Score {

	uint16_t current { 0 };
	// Highscore is read from the EEPROM non-volatile memory.
	uint16_t high 	 { ((EEPROM.read(0) != 255) ? static_cast<uint16_t>(EEPROM.read(0) * 10) : 0) };
}



// Define a button.
struct Button {

	enum class State { pressed, notPressed };

	constexpr static uint8_t readingPeriod_ms { 1 }; 	// Time between button reads.
	constexpr static uint8_t triggerCount { 3 }; 		// Number of reads required to trigger a press. 

	constexpr Button(uint8_t pin, Direction direction) : pin{ pin }, direction{ direction } {}

	const uint8_t pin;			// The pin of the button.
	const Direction direction;	// The direction the button represents.

	mutable State state = State::notPressed;
	mutable uint8_t pressedCount { 0 };
	mutable uint8_t unPressedCount { 0 };
};


// Create buttons.
namespace Buttons {

	constexpr Button 	bUp		{ Pin::UP, 		Direction::UP },
						bDown	{ Pin::DOWN, 	Direction::DOWN },
						bLeft	{ Pin::LEFT, 	Direction::LEFT },
						bRight	{ Pin::RIGHT, 	Direction::RIGHT },
						bMiddle { Pin::MIDDLE,	Direction::MIDDLE };

	constexpr Button const* All[] { &bUp, &bDown, &bLeft, &bRight, &bMiddle };
}


// Sets the pace of the game.
namespace Timing {

	uint16_t gameUpdateTime_ms { 300 };						// This decreases as you score points.
	constexpr uint16_t gameUpdateTimeOnReset_ms { 300 };	// This is the value it resets to.
	unsigned long lastGameUpdatedTime { 0 };				// A counter used in the loop.
}

namespace Game {
	enum class State {
		EntrySplash, Running, Paused, GameOver, Error
	};
	volatile State state{ State::EntrySplash }; 
}



namespace Error {

//Adafruit_SSD1306* displayPtr;
void initErrors(Adafruit_SSD1306& display);
void displayError(int16_t line, const char* file, const char* msg);
//void displayError(int16_t line, const char* file, const __FlashStringHelper* msg);

}





// ---------------------------------------------------
// -------------- Function Declarations --------------
// ---------------------------------------------------
/**
 * @brief Check if food eaten.
 * @return true if yes.
 */
inline bool detectPlayerAteScran();

/**
 * @brief Check if the player left the game area.
 * @param newHead A point describing where the new head will be.
 * @return true if out of area else false.
 */
bool detectPlayerOutOfArea(const PointType& newHead);

/**
 * @brief Check if the player collided with himself.
 * @param newHead A point describing where the new head will be.
 * @return true if a collision is deteced else false.
 */
bool detectSelfCollision(const PointType& newHead);

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
 * @param wholeSnake A boolean value. If true draws every segment else by default or if false optimizes the operation.
 */
void drawSnake(bool wholeSnake = false);

/**
 * @brief  Redraw all objects in the game world.
 */
void redrawAll();

/**
 * @brief Place food at a random location and draw.
 */
inline void placeRandomScran();

/**
 * @brief Read and debounce a button.
 * @param Button Pointer to the button to be read.
 */
void readButton(Button const* button);

/**
 * @brief Read all of the buttons.
 */
void readButtons();

/**
 * @brief Reset the snake, score and food.
 */
void resetGameParameters();

/**
 * @brief The game loop.
 */
void updateGame();

/**
 * @brief Draws the updated score.
 */
void drawUpdatedScore();

/**
 * @brief Wait for a press at the start of the game.
 */
void doSplashScreen();

/**
 * @brief Display a paused message and pause.
 */
void doPaused();

/**
 * @brief Does a fancy new high score animation.
 */
void doHighScore();


// New drawing utility functions.

void clear() { Display::display.clearDisplay(); }

template <typename PointT>
void drawFilledRect(const Rectangle<PointT>& r, uint16_t COLOUR = WHITE) {
	auto& d = Display::display;
	d.fillRect(r.origin().x, r.origin().y, r.width(), r.height(), COLOUR);
}

template <typename PointT>
void drawRndFilledRect(const Rectangle<PointT>& r, int16_t radius, uint16_t COLOUR = WHITE) {
	auto& d = Display::display;
	d.fillRoundRect(r.origin().x, r.origin().y, r.width(), r.height(), radius, COLOUR);
}

template <typename PointT>
void drawRect(const Rectangle<PointT>& r, uint16_t COLOUR = WHITE) { 
	auto& d = Display::display;
	d.drawRect(r.origin().x, r.origin().y, r.width(), r.height(), COLOUR); 
}

template <typename PointT>
void drawRndRect(const Rectangle<PointT>& r, int16_t radius, uint16_t COLOUR = WHITE) {
	auto& d = Display::display;
	d.drawRoundRect(r.origin().x, r.origin().y, r.width(), r.height(), radius, COLOUR);
}

template <typename PointT, typename DataT = decltype(PointT::x)>
auto getTextBoundsRect(const char* text, int16_t y, int16_t x) -> Rectangle<PointT> {
		
	auto& d = Display::display;
	int16_t x1, y1;
	uint16_t h, w;
	d.getTextBounds(text, x, y, &x1, &y1, &w, &h);
	return Rectangle<PointT> { {static_cast<DataT>(y1), static_cast<DataT>(x1)}, { static_cast<DataT>(h), static_cast<DataT>(w) }};
}


#if (DEBUG == YES)
/**
 * @brief Convert directions to string.
 * @param d The direction to convert.
 * @returns String represenatation of direction.
 */
const __FlashStringHelper* directionAsString(Direction d);

/**
 * @brief Convert game state to a string.
 * @param s The game state to convert.
 * @return const __FlashStringHelper*.
 */
const __FlashStringHelper* stateAsString(Game::State s);
#endif



//	Setup. Called from Arduino API.  Runs once when the program starts.
void setup() {

	using namespace Display;
	delay(Timing::gameUpdateTime_ms);

// Initialize interrupt timer for reading the buttons.
	ITimer1.init();
	ITimer1.attachInterruptInterval(Button::readingPeriod_ms, readButtons);

// Seed the random function with a random value.
	randomSeed(analogRead(0));

// Initialize the display.
	display.begin(SSD1306_SWITCHCAPVCC, Address);

#if (DEBUG == YES)
	Serial.begin(9600);
#endif // (DEBUG == YES)
#if (LIVE_ERRORS == YES) // Allows Errors to be displayed on screen.
	Error::initErrors(display);
#endif // (LIVE_ERRORS == YES)
#if (CLEAR_HIGH_SCORE == YES)
	EEPROM.update(0, 0);
#endif // (CLEAR_HIGH_SCORE == YES)

    delay(Timing::gameUpdateTime_ms);
	// DEBUG_PRINT_FLASH("Size: ("); DEBUG_PRINT(World::maxX);
	// DEBUG_PRINT_FLASH(", "); DEBUG_PRINT(World::maxY);
	// DEBUG_PRINTLN_FLASH(")");

    clear(); 								// start with a clean display
    display.setTextColor(WHITE);			// set up text color rotation size etc  
    display.setRotation(0); 
    display.setTextWrap(false);
    display.dim(false);         			// set the display brighness

	// Setup the buttons pins.
	for (const auto* button : Buttons::All) {
		pinMode(button->pin, INPUT_PULLUP);
	}
    
	DEBUG_PRINTLN_FLASH("Setup Complete");
	DEBUG_PRINT_FLASH("GCC Version: ");
	DEBUG_PRINTLN(__VERSION__);
	DEBUG_PRINT_FLASH("C++ Version: ");
	DEBUG_PRINTLN(__cplusplus);
	
	pinMode(5, OUTPUT);
	pinMode(6, OUTPUT);

    doSplashScreen();    		// display the snake start up screen
}

#if (DEBUG == YES)
static uint16_t counter{};
#endif

// Main Loop called from the Arduino API.
void loop() {

	auto tNow { millis() };

	// Game Loop
	if (tNow - Timing::lastGameUpdatedTime > Timing::gameUpdateTime_ms) {
//		DEBUG_PRINTLN_FLASH("SNAKE AT START:"); DEBUG_PRINTLN(snake);
		DEBUG_PRINT_FLASH("Turn: "); DEBUG_PRINTLN(++counter); 
		if 		(Game::state == Game::State::Running) 	updateGame();
		else if (Game::state == Game::State::Paused) 	doPaused();
		else if (Game::state == Game::State::Error) { 
			#pragma warning "uncomment" // Error::displayError(__LINE__, __FILE__, "In Error State");
			DEBUG_PRINT_FLASH("Error");
		}

//		DEBUG_PRINTLN_FLASH("SNAKE AT END:"); DEBUG_PRINTLN(snake); 
		Timing::lastGameUpdatedTime = tNow;
		DEBUG_PRINTLN();
	}
}


void readButton(Button const* button) {
	
	if (!digitalRead(button->pin) ) {
		button->unPressedCount = 0;

		if (++button->pressedCount >= Button::triggerCount / Button::readingPeriod_ms) {

			if (button->state != Button::State::pressed) {

				// if (button == &Buttons::bMiddle && Game::state != Game::State::EntrySplash) {
				// 	DEBUG_PRINTLN_FLASH("Middle button pressed.");
				// 	if (Game::state == Game::State::Running)
				// 		Game::state = Game::State::Paused;
				// 	else if (Game::state == Game::State::Paused)
				// 		Game::state = Game::State::Running;
				// } else if (Game::state == Game::State::EntrySplash)
				// 		lastDirectionPressed = Direction::RIGHT;
				// } else { 
				// 	lastDirectionPressed = button->direction;
				// }
				lastDirectionPressed = button->direction;
				button->state = Button::State::pressed;
			}
		}
	} else if (button->state == Button::State::pressed && (++button->unPressedCount >= Button::triggerCount / Button::readingPeriod_ms)) {

		button->state = Button::State::notPressed;
		button->pressedCount = 0;
	}
}



// This is called by the timer interrupt.
void readButtons() {

	digitalWrite(6, HIGH);

	using namespace Timing;
	using namespace Game;

	for (auto const& button : Buttons::All) readButton(button);

	// We set paused here so that it happens quickly.
	if (lastDirectionPressed == Direction::MIDDLE && state == State::Running) {
		state = State::Paused;
		lastDirectionPressed = Direction::NONE;
	}

	// switch (state) {
	// 	case State::EntrySplash:
	// 	case State::Running:
	// 		for (auto& button : Buttons::All) readButton(button);
	// 		break;
	// 	case State::Paused:
	// 		readButton(&Buttons::bMiddle);
	// 		break;
	// 	case State::GameOver:
	// 		break;
	// 	default:
	// 		state = Game::State::Error; // Unhandled game state.
	// }

	digitalWrite(6, LOW);
}



void resetGameParameters() {

	lastDirectionPressed = Direction::NONE;
	snake = SnakeType { }; 					// Create a new empty snake.
	snake.push( World::getRandomPoint() ); 	// Put the snake in a random place.

	Score::current = 0;						// Reset the score.
	Timing::gameUpdateTime_ms = Timing::gameUpdateTimeOnReset_ms; // Reset game speed.

	placeRandomScran();						// Place the food.
}



void drawDisplayBackground() {

	using namespace Display;

	display.setTextSize(0);
	display.setTextColor(WHITE);	
	
	// draw scores
	display.setCursor(2, 1);
	display.print(F("Score:"));
	display.print(Score::current);
	
	display.setCursor((dspRect.width() / 2) + 2, 1);	
	display.print(F("High:"));
	display.print(Score::high);

	// draw play area
	//      pos  1x, 1y, 2x, 2y, colour
	display.drawLine(0, 0, dspRect.width() - 1, 0, WHITE); 						// very top border
	display.drawLine((dspRect.width() / 2) - 1, 0, (dspRect.width() / 2) - 1, 9, WHITE); 	// score seperator
	display.fillRect(0, 9, dspRect.width() - 1, 2, WHITE); 						// below text border
	display.drawLine(0, 0, 0, 9, WHITE);
	display.drawLine(dspRect.width() - 1, 0, dspRect.width() - 1, 9, WHITE);

	display.fillRect(0, dspRect.height() - 3, dspRect.width() - 1, 3, WHITE);	// bottom border
	display.fillRect(0, 9, 3, dspRect.height() - 1, WHITE); 					// left border
	display.fillRect(dspRect.width() - 3, 9, 3, dspRect.height() - 1, WHITE); 	// right border    
}


void updateGame() {

// Current order of events.
// 1. - If direction is changed then change direction.
// 2. - If snake moving then determine new head position.
// 3. - Detect if out of area or self collision.  If not add a new head.
// 4. - Detect if the player ate scran.
// 5. - If scran eaten then update the score. else pop the tail and rub out the tail.
// 6. - Draw the snake.
// 7. - If scran eaten then replace the scran.
// 8. - Update the display.
    
	using namespace Display;
	auto scranEaten { false };

// Update the Snake's direction from button input if not same or opposite direction.
	DEBUG_PRINTLN(directionAsString(lastDirectionPressed));

	if (lastDirectionPressed != snake.getDirection()) {
		DEBUG_PRINTLN(directionAsString(~lastDirectionPressed));
		if (lastDirectionPressed != ~snake.getDirection())
			snake.setDirection(lastDirectionPressed);
	}

// Save current head position.
	const auto currentHead = snake.head();

// If the snake is moving.
	if (snake.getDirection() != Direction::NONE) { 

		// Move the Snake.
		PointType newHead {};

		switch (snake.getDirection()) { 
			case Direction::UP:
				newHead = {static_cast<POINT_DATA_TYPE>(currentHead.y - 1), currentHead.x };
				break;
			case Direction::DOWN:
				newHead = { static_cast<POINT_DATA_TYPE>(currentHead.y + 1), currentHead.x };
				break;
			case Direction::LEFT:
				newHead = { currentHead.y, static_cast<POINT_DATA_TYPE>(currentHead.x - 1) };
				break;
			case Direction::RIGHT:
				newHead = { currentHead.y, static_cast<POINT_DATA_TYPE>(currentHead.x + 1) };
				break;
			default: break;
		}
		//DEBUG_PRINTLN((snake.pointIsInside(newHead))?"true":"false");
		if (detectPlayerOutOfArea(newHead) || detectSelfCollision(newHead)) {	
			doGameOver();
			return;
		} else snake.push(newHead);

		scranEaten = detectPlayerAteScran();
		
		if (scranEaten) { // If eating tail stays put and only head advances.
			drawUpdatedScore();
		} else {
			auto removed = snake.pop();

			// best place to remove the tail.
			display.fillRect((removed.x * World::Scale) + World::xMinOffset, (removed.y * World::Scale) + World::yMinOffset, World::Scale, World::Scale, BLACK);
		}
	}
	//DEBUG_PRINTLN_FLASH("Draw");
	drawSnake();
	if (scranEaten) { placeRandomScran(); }
	display.display();
}


void doSplashScreen() {

	using namespace Display;
	Game::state = Game::State::EntrySplash;
	clear();

	while (Game::state == Game::State::EntrySplash) {

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
				//DEBUG_PRINTLN_FLASH("Resetting Game Parameters.");
			resetGameParameters();
			redrawAll();
			Game::state = Game::State::Running;
		}
	}
}


void drawARandomLine(uint8_t colour) {

	auto getRand { [](uint8_t max) -> uint8_t { return static_cast<uint8_t>(random(0, max)); } };

	PointType start { getRand(Display::dspRect.maxY()), getRand(Display::dspRect.maxX()) };
	PointType end   { getRand(Display::dspRect.maxY()), getRand(Display::dspRect.maxX()) };
	
	Display::display.drawLine(start.x, start.y, end.x, end.y, colour);
}


void drawScran() {

	auto& d = Display::display;
	using namespace World;
	d.drawRect ( ( scranPos.x * Scale ) + xMinOffset,
				 ( scranPos.y * Scale ) + yMinOffset,
				   Scale,
				   Scale,
				   WHITE );

	// Circles just don't work.
	// d.drawCircle(	( scranPos.x * Scale) + xMinOffset + (Scale / 2),
	// 				( scranPos.y * Scale) + yMinOffset + (Scale / 2),
	// 				Scale / 2,
	// 				WHITE
	// 			);
}


void drawSnake(bool wholeSnake) {

	using namespace World;
	using namespace Display;

	auto headPos = toWorld(snake.head());

	// draw the head.
	display.fillRect( headPos.x, headPos.y, Scale, Scale, WHITE );

	// We don't want to draw all.  We want to draw the one after the head. and the last 2.
	if (snake.length() == 1) return;
	if (snake.length() > 1) {
		auto tailPos = toWorld(snake.tail());
		display.fillRect( tailPos.x, tailPos.y, Scale, Scale, BLACK);
		display.fillRect( tailPos.x + 3 , tailPos.y + 3, Scale - 3, Scale - 3, WHITE);
	}
	if (snake.length() > 2) {
		auto pos = toWorld(snake[snake.length() - 2]);
		display.fillRect( pos.x, pos.y, Scale, Scale, BLACK);
		display.fillRect( pos.x + 2, pos.y + 2, Scale - 2, Scale - 2, WHITE);
	}
	if (snake.length() > 3) {
		auto pos = toWorld(snake[1]);
		display.fillRect( pos.x, pos.y, Scale, Scale, BLACK);
		display.fillRect( pos.x + 1, pos.y + 1, Scale - 1, Scale - 1, WHITE);
	}

	if (wholeSnake) {
		for (uint16_t i = 0; i < snake.length() - 1; ++i) {
			
			auto pos = toWorld(snake[i]);				
			if (i == snake.length() - 2) {
			} else if (i == snake.length() - 1) {
			} else {
				display.fillRect(pos.x + 1, pos.y + 1, Scale - 1, Scale - 1, WHITE);
			}
		}
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
	do {
		scranPos = getRandomPoint();
		DEBUG_PRINT_FLASH("scranpos: "); DEBUG_PRINTLN(scranPos);
	} while (snake.pointIsInside(scranPos));

	// Draw scran here cause only want to draw one time.
	drawScran();
}


void redrawAll() {

	using namespace Display;
	clear();
	drawDisplayBackground();
	drawScran();
	drawSnake(true);
	display.display();
}


//#pragma message "detect ate scran and related game updates should be separate."
bool detectPlayerAteScran() {
	if (snake.head() == World::scranPos) {

		//DEBUG_PRINTLN_FLASH("Player ate scran");
		Score::current += 10;

		if (Score::current % 100 == 0)
			Timing::gameUpdateTime_ms -= (Timing::gameUpdateTime_ms / 10);            
		
		tone(Pin::SOUND, 2000, 10);
		return true;
	}
	return false;
}



bool detectSelfCollision(const PointType& newHead) {

	auto hasCollided { snake.pointIsInside(newHead) };

	if (hasCollided && hasCollided.getValue() != snake.tail()) {

			tone(Pin::SOUND, 2000, 20);
			tone(Pin::SOUND, 1000, 20);
			DEBUG_PRINT_FLASH("Detected self collision at: "); 
			DEBUG_PRINTLN(hasCollided.getValue());
			DEBUG_PRINTLN(snake);
			return true;
	}
	return false;
}



bool detectPlayerOutOfArea(const PointType& newHead) {

	using World::World;

#if (DEBUG == 1)
	bool rVal;
	if constexpr(Utility::is_unsigned<POINT_DATA_TYPE>::value)
		rVal = (( newHead.x >= World.maxX()  ) || ( newHead.y >= World.maxY() ));
	else 
		rVal = (( newHead.x >= World.maxX() ) || ( newHead.x < 0 ) || 
				( newHead.y >= World.maxY() ) || ( newHead.y < 0 ));
	
	if (rVal) {

		//DEBUG_PRINT_FLASH("head pos: ");
		//PointType newHeadScaled { static_cast<uint8_t>(newHead.y / World::Scale), static_cast<uint8_t>(newHead.x / World::Scale) };
		//DEBUG_PRINTLN(newHeadScaled);
		//DEBUG_PRINT_FLASH("World min/max: ");
		//PointType WorldMin { World.minY(), World.minX() };
		//DEBUG_PRINT(WorldMin);
		//PointType WorldMax { World.maxY(), World.maxX() };
		//DEBUG_PRINT_FLASH(", ");
		//DEBUG_PRINTLN( WorldMax );
		DEBUG_PRINTLN_FLASH("Detected out of area");
	}
	return rVal;
#else
	if (Utility::is_unsigned<POINT_DATA_TYPE>::value)
		return (( newHead.y >= World.maxY() ) || ( newHead.x >= World.maxX() ));
	else return (( newHead.x >= World.maxX() ) || ( newHead.x < 0 ) ||
				( newHead.y >= World.maxY() || newHead.y < 0 ));
#endif
}



void doGameOver() {
    
	using namespace Display;
	using namespace World;
	Game::state = Game::State::GameOver;

	// Flash the snake
	bool on { false };
	uint8_t dly { 60 };

	for (uint8_t i{}; i < 17; ++i) {
		if (!on) 
			for (uint16_t j{}; j < snake.length(); ++j) {
				auto pos = toWorld(snake[j]);
				display.fillRect(pos.x, pos.y, Scale, Scale, BLACK);
			}
		else 
			drawSnake(true);

		display.display();
		on = !on;
		delay(dly);
		dly -= 4;
	}
	delay(350);

	clear();
    display.setCursor(40, 30);
    display.setTextSize(1);
    
	tone(Pin::SOUND, 2000, 50);
    display.print(F("GAME OVER"));
    delay(500);
	tone(Pin::SOUND, 1000, 50);
//    display.print(F("OVER"));

	uint8_t rectX1 { 38 }, rectY1 { 28 }, rectX2 { 58 }, rectY2 { 12 };

    for (uint8_t i{}; i <= 16; ++i) { // this is to draw rectangles around game over

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

	for (uint8_t i{0}; i <= 64; i++) {
		
		display.drawLine(rectX1, rectY1, rectX2, rectY2, BLACK); 
		++rectX1;
		++rectX2;
		display.drawLine(rectX1, rectY1, rectX2, rectY2, BLACK);
		++rectX1;
		++rectX2;

		display.display();                          
    }

	if (Score::current > Score::high) {
		Score::high = Score::current;
		doHighScore();
		EEPROM.write(0, Score::high / 10);
	}
    
	lastDirectionPressed = Direction::NONE;
	doSplashScreen();		// wait for player to re-start game
}


void doPaused() {

	using namespace Display;

	// Draw a box.
	SizeType boxSize { 20, 76 };
	display.fillRect((dspRect.width() / 2) - (boxSize.x / 2) , (dspRect.height() / 2) - (boxSize.y / 2), boxSize.x, boxSize.y, BLACK);
	display.drawRect((dspRect.width() / 2) - (boxSize.x / 2) , (dspRect.height() / 2) - (boxSize.y / 2), boxSize.x, boxSize.y, WHITE);

	// Write paused.
	display.setTextSize(2);
	display.setCursor((dspRect.width() / 2) - (boxSize.x / 2) + 3 , (dspRect.height() / 2) + 3 - (boxSize.y / 2));
	display.print(F("Paused"));

	// Display
	display.display();

	// Wait while paused.
	while(Game::state == Game::State::Paused) {
		if (lastDirectionPressed == Direction::MIDDLE) {
			Game::state = Game::State::Running;
			lastDirectionPressed = snake.getDirection();
			break;
		}
	}

	// Redraw everything.
	display.setTextSize(1);

	display.fillRect((dspRect.width() / 2) - (boxSize.x / 2) , (dspRect.height() / 2) - (boxSize.y / 2), boxSize.x, boxSize.y, BLACK);

	drawSnake(true);
	drawScran();
	display.display();
}


void doHighScore() {

	// Try with 4 rectangles.
	using World::World;
	using namespace Display;

// Control variables
	enum class Stage { Waiting, Growing, DisplayText };
	Stage stage { Stage::Waiting };

// For the outer pattern.	
	bool flipped { false };
	SizeType growthSpeed { 3, 6 };
	
// The text to display.
	display.setTextSize(3);
	char text[6] { " New " };
	Rect textRect {};
	const auto finalSize { getTextBoundsRect<Point<uint8_t>>("Score", 0, 0).grow(5, 6).size() };

//	DEBUG_PRINTLN(finalSize);
//	DEBUG_PRINTLN(textRect);
	
	clear();
	display.display();
	const auto startTime { millis() };

	while (true) {

		auto timeElapsed { millis() - startTime };
		if (timeElapsed > 9000) break;
		else if (timeElapsed > 5600) sprintf(text, "%u", Score::high);
		else if (timeElapsed > 4100) sprintf(text, "Score");
		else if (timeElapsed > 3500) sprintf(text, "High");
		else if (timeElapsed > 2800) stage = Stage::DisplayText;
		else if (timeElapsed > 1000) stage = Stage::Growing;


		Rect rInner{};
		Rect rOuter {{dspRect.height() >> 1, dspRect.width() >> 1}};

		for (uint8_t i{0}; rOuter.width() <= dspRect.width(); ++i) {

			rOuter.centreOn(dspRect);
			rInner.centreOn(dspRect);

			drawFilledRect(rOuter, flipped);
			drawFilledRect(rInner, !flipped);

			auto outline { textRect };
			drawRndRect(outline, 7, BLACK);
			outline.grow(-1);
			drawRndRect(outline, 7, BLACK);
			drawRndFilledRect(textRect, 5);

			rInner.grow(growthSpeed.y, growthSpeed.x);
			rOuter.grow(growthSpeed.y, growthSpeed.x);


			switch (stage) {
				case Stage::Waiting:
					break;
				case Stage::Growing:
					if (textRect.width() < finalSize.x) {
						if (textRect.height() < finalSize.y) 
							textRect.grow(1, 1);
						else textRect.grow(0, 3); 
						
						textRect.centreOn(dspRect);
						DEBUG_PRINTLN(textRect);
					}
					break;
				case Stage::DisplayText: {

						display.setTextColor(BLACK);
						auto bounds { getTextBoundsRect<Point<uint8_t>>(text, 0,0) };
						bounds.centreOn(dspRect);

						display.setCursor(bounds.origin().x, bounds.origin().y);
						display.write(text);
						break;
				}
				default: break;
			}
			display.display();
		}
		flipped = !flipped;
	}
	display.setTextSize(1);
	delay(1000);
}


#if (DEBUG == YES)
const __FlashStringHelper* directionAsString(Direction d) {

	switch (d) {
		case Direction::UP:		return F("Up"); break;
		case Direction::DOWN: 	return F("Down"); break;
		case Direction::LEFT: 	return F("Left"); break;
		case Direction::RIGHT: 	return F("Right"); break;
		case Direction::NONE: 	return F("None"); break;
		case Direction::MIDDLE: return F("Middle"); break;
		default:				return F("Error");
	}
}

const __FlashStringHelper* stateAsString(Game::State s) {

	using namespace Game;
	
	switch (s) {
		case State::EntrySplash: 	return F("Entry"); break;
		case State::Paused:			return F("Pause"); break;
		case State::Running:		return F("Run"); break;
		case State::GameOver:		return F("Over"); break;
		case State::Error:			
		default: 					return F("Error");
	}
}

#endif





Direction CrumbPtr::getValue() { 
	return static_cast<Direction>((*ptr >> ((3 - crumb) << 1)) & 0x03);
}
  
void CrumbPtr::putValue(Direction newval) {
	*ptr &= ~(0x03 << ((3 - crumb) << 1));
	*ptr |= (static_cast<uint8_t>(newval) << ((3 - crumb) << 1));      
}


bool CrumbPtr::operator==(const CrumbPtr& other) const {
	return ((this->ptr == other.ptr) && (this->crumb == other.crumb));
}

bool CrumbPtr::operator!=(const CrumbPtr& other) const {
	return !(*this == other);
}  


// Prefix
CrumbPtr::selfType& CrumbPtr::operator++() { 
	if (crumb == 3) { crumb = 0; ptr++; }
	else crumb++; 
	return *this;
}
	
// Postfix
CrumbPtr::selfType CrumbPtr::operator++(int) {
    selfType rVal{*this};
    ++(*this);
	return rVal;
}

// Prefix
CrumbPtr::selfType& CrumbPtr::operator--() {
	if (crumb == 0) { crumb = 3; ptr--; }
	else crumb--;
	return *this; 
}

// Postfix
CrumbPtr::selfType CrumbPtr::operator--(int) {
	selfType rVal {*this};
	--(*this);
	return rVal;
}

#if (DEBUG == YES)
size_t CrumbPtr::printTo(Print& p) const {
	char addr[10];
	sprintf(addr, "%p:%u", ptr, crumb);
	return p.print(addr);
}

#endif // (DEBUG == YES)


Direction operator~(const Direction direction) {
	// Return inverted if in range 0-3;
	if (static_cast<uint8_t>(direction) > 0x03) return static_cast<Direction>(0x4);
    return static_cast<Direction>(~static_cast<uint8_t>(direction) & 0x03);
}

// Use __FlashStringHelper* DirectionAsString instead
//#if (DEBUG == YES)
// String DirectionAsString(const Direction d) {
// 	switch (d) {
// 		case Direction::UP: 	return "UP"; break;
// 		case Direction::LEFT: 	return "LEFT"; break;
// 		case Direction::RIGHT: 	return "RIGHT"; break;
// 		case Direction::DOWN: 	return "DOWN"; break;
// 		case Direction::NONE: 	return "NONE"; break; 
// 		default: return "ERROR";
// 	}
// }
//#endif // (DEBUG == YES)

//namespace Utility {
// #if (DEBUG == YES)
// // This class allows to print any integer type in binary format.
// //  It is here to aid in debugging. 
// template<typename T>
// struct Binary : public Printable {
//     T value;
// 	constexpr Binary(T value) : value{value} { } 
// 	size_t printTo(Print& p) const;
// };
// #endif // (DEBUG == YES)
//}


