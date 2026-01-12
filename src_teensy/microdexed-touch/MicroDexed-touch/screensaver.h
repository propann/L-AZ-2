#ifndef _SCREENSAVER_H
#define _SCREENSAVER_H

enum ScreenSaver {
  RANDOM            = 0,
  QIX               = 1,
  CUBE              = 2,
  SWARM             = 3,
  TERRAIN           = 4,
  GAMEOFLIFE        = 5,
  CM5_I             = 6,
  CM5_II            = 7,
  ASTERO            = 8,
  DISABLED          = 9,
  NUM_SCREENSAVERS  = 10
};

void boot_animation();
void InitializeCube();
void cube_screensaver();
void qix_screensaver();
#endif

#ifndef ASTEROIDS_SCREENSAVER_H
#define ASTEROIDS_SCREENSAVER_H

class AsteroidsScreensaver {
public:
  AsteroidsScreensaver();
  void begin();
  void update();

private:
  // Constants
  static const int MAX_ASTEROIDS = 30;
  static const int MAX_SHOTS = 6;
  static const int MAX_POINTS = 12;
  static constexpr float MIN_SPEED = 0.2f;
static constexpr float MAX_SPEED = 1.0f;
  static const int STARTING_ASTEROIDS = 6;
  static const int MIN_ASTEROIDS = 6;
  const float THRUST_POWER = 0.15f;
  const float MAX_SHIP_SPEED = 1.7f;
  const float FUEL_CONSUMPTION = 100.0f / (2.0f * 30.0f);
  const float SHIELD_RECHARGE_RATE = 0.2f;
  const float SHIELD_DAMAGE = 25.0f;
  int score;

  const int LARGE_ASTEROID_SCORE = 5;
  const int MEDIUM_ASTEROID_SCORE = 2;
  const int SMALL_ASTEROID_SCORE = 1;

  enum AsteroidSize { LARGE, MEDIUM, SMALL };
  
  struct Asteroid {
    float centerX, centerY;
    float prevCenterX, prevCenterY;
    float vx, vy;
    float rotation;
    float prevRotation;
    float rotationSpeed;
    int numPoints;
    float relX[MAX_POINTS];
    float relY[MAX_POINTS];
    bool active;
    AsteroidSize size;
    float radius;
    uint8_t hitPoints;
    bool flash;
  };
  
  struct LaserShot {
    int x, y;
    int prevX, prevY;
    float vx, vy;
    bool active;
  };
  
  struct Spaceship {
    float x, y;
    float prevX, prevY;
    float angle;
    float prevAngle;
    float size;
    uint8_t cooldown;
    LaserShot shots[MAX_SHOTS];
    bool shotActive;
    float vx, vy;
    float thrusterFuel;
    float shield;
    bool thrusting;
    bool prevThrusting;
    bool wasHit;
  };

  Asteroid asteroids[MAX_ASTEROIDS];
  Spaceship ship;
  uint8_t activeAsteroids;
  void initAsteroidShape(Asteroid &a);
  void initAsteroid(Asteroid &a, AsteroidSize size);
  void updateAsteroid(Asteroid &a);
  void drawAsteroid(Asteroid &a, uint16_t color);
  void spawnAsteroidFromBorder();
  void updateSpaceship();
  void splitAsteroid(int index);
  void fireShot();
  void drawSpaceship(uint16_t color);
  void drawShot(LaserShot &shot);
  void drawFuelGauge();
  void drawShieldGauge();
  void drawScore();
  void savePreviousState();
  void erasePreviousState();
};

#endif