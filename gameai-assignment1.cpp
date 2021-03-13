#include <iostream> //for debugging
#include "vec.hpp"
#include "draw-triangle-pro.hpp"
#include "raylib-cpp.hpp"
#include <vector>
#include <cmath>  // std::atan, std::fmod, std::abs
#include <random> // std::random_device, std::mt19937
#include <algorithm> // std::clamp
#include <variant> // std::variant, std::visit

using Vector = ai::Vector3;  // use x and z in the 2D case

//this function creates a random float between -1f and 1f which represent the platform of the hunt when we multiply it by the size of the map
float randomB()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    return dis(gen)-dis(gen);
}

// These two values each represent an acceleration.
// i.e. they effect changes in velocity (linear and angular).
class SteeringOutput
{
public:
  Vector linear_;
  float angular_;

  SteeringOutput &operator+=(const SteeringOutput &rhs)
  {
    linear_ += rhs.linear_;
    angular_ += rhs.angular_;
    return *this;
  }
  friend SteeringOutput operator*(const float lhs, const SteeringOutput &y) {
    return {lhs*y.linear_, lhs*y.angular_};
  }

};



class Kinematic
{
public:
  Vector position_;
  float orientation_;
  Vector velocity_;
  float rotation_;

  // create a random Kinematic that can be useful in the code at some point
  Kinematic* RandomK() 
  {
      float a = randomB(); float b = randomB(); float c = randomB();
      this->position_ = Vector(a,b,c);
      this->orientation_ = randomB();
      a = randomB(); b = randomB(); c = randomB();
      this->velocity_ = Vector(a, b, c);
      this->rotation_ = randomB();
      return this;
  }


  // integration of the linear and angular accelerations
  void update(const SteeringOutput& steering,
              const float maxSpeed,
              float drag,
              const float time) // delta time
  {
    //Newton-Euler 1 simplification:
      velocity_ += steering.linear_ * time;
      rotation_ += steering.angular_ * time;

    position_ += velocity_ * time;
    /*
    I exchanged the post process and orientation because post process was "eating" the wander orientation 
    each update making it impossible to be done properly except by changing the update function or create a new one. It works correctly so I just changed it for now.
    */
    post_process(drag, maxSpeed, time);

    orientation_ += rotation_ * time;
    orientation_ = std::fmod(orientation_, 2*PI); // (-2pi,2pi) - not crucial?

    
  }

  void post_process(const float drag, const float maxSpeed, const float time)
  {
    if (velocity_.length() > 0)
      orientation_ = std::atan2(-velocity_.x, velocity_.z);

    velocity_ *= (1 - drag * time);
    rotation_ *= (1 - drag * time);

    if (velocity_.length() > maxSpeed)
    {
      velocity_.normalise();
      velocity_ *= maxSpeed;
    }
  }
};


class Ship
{
public:
  Ship(const float z, const float x, const float ori, const raylib::Color col)
    : k_{{x,0,z},ori,{0,0,0},0}, col_{col} { }

  Kinematic k_;
  raylib::Color col_;

  void draw(int screenwidth, int screenheight)
  {
    const float w = 10, len = 30; // ship width and length
    const ai::Vector2 l{0, -w}, r{0, w}, nose{len, 0};
    ai::Vector2 pos{k_.position_.z, k_.position_.x};
    float ori = -k_.orientation_ * RAD2DEG; // negate: anticlockwise rot

    ai::DrawTrianglePro(pos, l, r, nose, ori, col_);
  }

  //Makes the ship reappear on the other side of the map if it crosses the screen walls
  void wrap(int width,int height)
  {
      float w = (float)width;
      float h = (float)height;

      if (this->k_.position_.x <= 0.0f)
      {
          this->k_.position_.x = h-0.01f;
      }

      if (this->k_.position_.x >= h)
      {
          this->k_.position_.x = 0.01f;
      }

      if (this->k_.position_.z <= 0.0f)
      {
          this->k_.position_.z = w-0.01f;
      }

      if (this->k_.position_.z >= w)
      {
          this->k_.position_.z = 0.01f;
      }

  }

  //pops a "new" prey when the hunter caught it and play a sound when caught
  void Caught(Ship ship, int w, int h, raylib::Sound& sound)
  {
      if ( (this->k_.position_.x - ship.k_.position_.x >= -10.0f && this->k_.position_.x - ship.k_.position_.x <= 10.0f) && (this->k_.position_.z - ship.k_.position_.z >= -10.0f && this->k_.position_.z - ship.k_.position_.z <= 10.0f))
      { 
          this->k_.position_.x = (float)(rand() %( h + 1 ));
          this->k_.position_.z = (float)(rand() %( w + 1 ));
          sound.Play();
          std::cout << "Caught" << std::endl;
      }
  }

};

/*Wandering Class Behaviour
* 
*  The targeted character will randomly wander on the map, it moves straight forward and the orientation angle change randomly for it not to move too weirdly.
*  A previous wander class was to target a position between (-10,-10) and (h+10,w+10) in coordonates but this doesn't allow your character to get outside the map
*  except if it was already poping on a border and randomly had a position in -x or h/w+x.
*  This version also could've make your character go back on their footsteps like "oh I forgot something brb" so I ended up following the Coursework version which was cleaner obviously.
*/
class Wander
{
public:
    Kinematic& character_;// The target we want to wander
    float maxAngle_; //The max rotation the target is allowed to turn 
    float maxAcceleration_; // The max speed the target will go

    // A constructor isn't needed, but adding it will also not hurt
    Wander::Wander(Kinematic& c,float maxAngle, float maxAcceleration)
        : character_{ c }, maxAngle_{ maxAngle }, maxAcceleration_{ maxAcceleration }
    {}



    SteeringOutput getSteering() const
    {   
        /* Previous WanderSteeringCode:
        Kinematic targetR;
        targetR.RandomK();
        
        if (targetR.position_.x < -10.0f)
        {
            targetR.position_.x = h - 0.01f;
        }

        if (targetR.position_.x > h+10.0f)
        {
            targetR.position_.x = 0.01f;
        }

        if (targetR.position_.z < -10.0f)
        {
            targetR.position_.z = w - 0.01f;
        }

        if (targetR.position_.z > w+10.0f)
        {
            targetR.position_.z = 0.01f;
        }
        if (IsKeyPressed(KEY_SPACE))
        {
            std::cout << targetR.position_.x << " " << targetR.position_.z << std::endl;
        }*/


        SteeringOutput result;

        result.linear_ = ai::asVector(character_.orientation_);
        result.linear_ *= maxAcceleration_;

        result.angular_ = randomB()*maxAngle_;
        //std::cout << result.angular_ << std::endl; //angular debugging test
        
        return result;
    }
};


// Dynamic Seek (page 96)
class Seek
{
public:
  Kinematic& character_;
  Kinematic& target_;

  float maxAcceleration_;

  // A constructor isn't needed, but adding it will also not hurt
  Seek::Seek(Kinematic &c, Kinematic &t, float maxAcceleration)
    : character_{c}, target_{t}, maxAcceleration_{maxAcceleration}
  {}

  SteeringOutput getSteering() const
  {
    SteeringOutput result;

    result.linear_ = target_.position_ - character_.position_;

    result.linear_.normalise();
    result.linear_ *= maxAcceleration_;

    result.angular_ = 0;
    return result;
  }
  

};

int main(int argc, char *argv[])
{
    //Sound and music initialization
  raylib::AudioDevice audiodevice; //Initialize the sound listening device
  raylib::Sound s1("../resources/weird.wav"); //Initialize the sound in a variable
  


  int w{1024}, h{768};
  raylib::Window window(w, h, "Game AI: Assignment 1");

  SetTargetFPS(60);

  Ship hunter{w/2.0f + 50, h/2.0f, 0, RED};
  Ship prey{w/2.0f + 250, h/2.0f + 300, 270*DEG2RAD, BLUE};

  float target_radius{5};
  float slow_radius{60};
  const float max_accel{200};
  const float max_ang_accel{10};
  const float max_speed{220};
  const float drag_factor{0.5};

  Seek seek{hunter.k_, prey.k_, 500}; //start chasing
  Wander wander(prey.k_,200, 500); //Start wander

  while (!window.ShouldClose()) // Detect window close button or ESC key
  {

    BeginDrawing();

    ClearBackground(RAYWHITE);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
      const auto mpos = GetMousePosition();
      /*
      sound test
      s1.Play();
      */
      std::cout << mpos.x << " " << mpos.y << std::endl;
    }

    prey.draw(w,h);
    hunter.draw(w,h);

   //activate the wrap 
    prey.wrap(w, h);
    hunter.wrap(w, h);
    

    EndDrawing();

    auto steer = seek.getSteering();

    prey.Caught(hunter,w,h,s1); //launchs the caught verification

    auto steer2 = wander.getSteering(); // steer for the wander action of the prey

    //std::cout << steer2.angular_<< std::endl;
    hunter.k_.update(steer, max_speed, drag_factor, GetFrameTime());
    prey.k_.update(steer2, max_speed - 60, drag_factor, GetFrameTime());
  }

  return 0;
}
