#include "config.h"
#include "screensaver.h"
#include "UI.h"
#include "ILI9341_t3n.h"
#include "midi_devices.h"

extern ILI9341_t3n display;
extern uint16_t ColorHSV(uint16_t hue, uint8_t sat, uint8_t val);
extern bool remote_active;

uint8_t screensaver_brightness = 255;
uint16_t screensaver_counthue = 0;

uint8_t const qix_num = 22;

typedef struct qix_s
{
  float x0s[qix_num], y0s[qix_num], x1s[qix_num], y1s[qix_num];
  float dx0 = 3.3, dx1 = 4.4, dy0 = 4.8, dy1 = 1.3;
} qix_t;

qix_s qix;

/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef Vector_H
#define Vector_H

template <class T>
class Vector2 {
public:
  T x, y;

  Vector2() :x(0), y(0) {}
  Vector2(T x, T y) : x(x), y(y) {}
  Vector2(const Vector2& v) : x(v.x), y(v.y) {}

  Vector2& operator=(const Vector2& v) {
    x = v.x;
    y = v.y;
    return *this;
  }

  bool operator==(Vector2& v) {
    return x == v.x && y == v.y;
  }

  Vector2 operator+(Vector2& v) {
    return Vector2(x + v.x, y + v.y);
  }
  Vector2 operator-(Vector2& v) {
    return Vector2(x - v.x, y - v.y);
  }

  Vector2& operator+=(Vector2& v) {
    x += v.x;
    y += v.y;
    return *this;
  }
  Vector2& operator-=(Vector2& v) {
    x -= v.x;
    y -= v.y;
    return *this;
  }

  Vector2& operator*=(double s) {
    x *= s;
    y *= s;
    return *this;
  }

  Vector2& operator/=(double s) {
    x /= s;
    y /= s;
    return *this;
  }

  Vector2& normalize() {
    if (length() == 0) return *this;
    *this *= (1.0 / length());
    return *this;
  }

  float dist(Vector2 v) const {
    Vector2 d(v.x - x, v.y - y);
    return d.length();
  }
  float length() const {
    return sqrt(x * x + y * y);
  }

  float mag() const {
    return length();
  }

  float magSq() {
    return (x * x + y * y);
  }

  void truncate(double length) {
    double angle = atan2f(y, x);
    x = length * cos(angle);
    y = length * sin(angle);
  }

  void limit(float max) {
    if (magSq() > max * max) {
      normalize();
      *this *= max;
    }
  }
};

typedef Vector2<float> PVector;

#endif

/*
 * Portions of this code are adapted from "Flocking" in "The Nature of Code" by Daniel Shiffman: http://natureofcode.com/
 * Copyright (c) 2014 Daniel Shiffman
 * http://www.shiffman.net
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

 // Flocking
 // Daniel Shiffman <http://www.shiffman.net>
 // The Nature of Code, Spring 2009

 // Boid class
 // Methods for Separation, Cohesion, Alignment added

static const int boidCount = 70;

class Boid {
public:

  PVector location;
  PVector velocity;
  PVector acceleration;

  float maxforce;    // Maximum steering force
  float maxspeed;    // Maximum speed

  float desiredseparation = 14;
  float neighbordist = 30;
  //float mass;

  Boid() {}

  Boid(float x, float y) {
    acceleration = PVector(0, 0);
    velocity = PVector(randomf(), randomf());
    location = PVector(x, y);
    maxspeed = 1.5;
    maxforce = 0.22;
  }

  static float randomf() {
    return mapfloat(random(0, 255), 0, 255, -.5, .5);
  }

  static float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }

  void run(Boid boids[]) {
    flock(boids);
    update();
    wrapAroundBorders();
    //bounceOffBorders();
  }

  // Method to update location
  void update() {
    // Update velocity
    velocity += acceleration;
    // Limit speed
    velocity.limit(maxspeed);
    location += velocity;
    // Reset acceleration to 0 each cycle
    acceleration *= 0;
  }

  void applyForce(PVector force) {
    // We could add mass here if we want A = F / M
    acceleration += force;
  }

  void repelForce(PVector obstacle, float radius) {
    //Force that drives boid away from obstacle.

    PVector futPos = location + velocity; //Calculate future position for more effective behavior.
    PVector dist = obstacle - futPos;
    float d = dist.mag();

    if (d <= radius) {
      PVector repelVec = location - obstacle;
      repelVec.normalize();
      if (d != 0) { //Don't divide by zero.
        // float scale = 1.0 / d; //The closer to the obstacle, the stronger the force.
        repelVec.normalize();
        repelVec *= (maxforce * 7);
        if (repelVec.mag() < 0) { //Don't let the boids turn around to avoid the obstacle.
          repelVec.y = 0;
        }
      }
      applyForce(repelVec);
    }
  }

  // We accumulate a new acceleration each time based on three rules
  void flock(Boid boids[]) {
    PVector sep = separate(boids);   // Separation
    PVector ali = align(boids);      // Alignment
    PVector coh = cohesion(boids);   // Cohesion
    // Arbitrarily weight these forces
    sep *= 1.5;
    ali *= 1.0;
    coh *= 1.0;
    // Add the force vectors to acceleration
    applyForce(sep);
    applyForce(ali);
    applyForce(coh);
  }

  // Separation
  // Method checks for nearby boids and steers away
  PVector separate(Boid boids[]) {
    PVector steer = PVector(0, 0);
    int count = 0;
    // For every boid in the system, check if it's too close
    for (int i = 0; i < boidCount; i++) {
      Boid other = boids[i];
      float d = location.dist(other.location);
      // If the distance is greater than 0 and less than an arbitrary amount (0 when you are yourself)
      if ((d > 0) && (d < desiredseparation)) {
        // Calculate vector pointing away from neighbor
        PVector diff = location - other.location;
        diff.normalize();
        diff /= d;        // Weight by distance
        steer += diff;
        count++;            // Keep track of how many
      }
    }
    // Average -- divide by how many
    if (count > 0) {
      steer /= (float)count;
    }

    // As long as the vector is greater than 0
    if (steer.mag() > 0) {
      // Implement Reynolds: Steering = Desired - Velocity
      steer.normalize();
      steer *= maxspeed;
      steer -= velocity;
      steer.limit(maxforce);
    }
    return steer;
  }

  // Alignment
  // For every nearby boid in the system, calculate the average velocity
  PVector align(Boid boids[]) {
    PVector sum = PVector(0, 0);
    int count = 0;
    for (int i = 0; i < boidCount; i++) {
      Boid other = boids[i];
      float d = location.dist(other.location);
      if ((d > 0) && (d < neighbordist)) {
        sum += other.velocity;
        count++;
      }
    }
    if (count > 0) {
      sum /= (float)count;
      sum.normalize();
      sum *= maxspeed;
      PVector steer = sum - velocity;
      steer.limit(maxforce);
      return steer;
    }
    else {
      return PVector(0, 0);
    }
  }

  // Cohesion
  // For the average location (i.e. center) of all nearby boids, calculate steering vector towards that location
  PVector cohesion(Boid boids[]) {
    PVector sum = PVector(0, 0);   // Start with empty vector to accumulate all locations
    int count = 0;
    for (int i = 0; i < boidCount; i++) {
      Boid other = boids[i];
      float d = location.dist(other.location);
      if ((d > 0) && (d < neighbordist)) {
        sum += other.location; // Add location
        count++;
      }
    }
    if (count > 0) {
      sum /= count;
      return seek(sum);  // Steer towards the location
    }
    else {
      return PVector(0, 0);
    }
  }

  // A method that calculates and applies a steering force towards a target
  // STEER = DESIRED MINUS VELOCITY
  PVector seek(PVector target) {
    PVector desired = target - location;  // A vector pointing from the location to the target
    // Normalize desired and scale to maximum speed
    desired.normalize();
    desired *= maxspeed;
    // Steering = Desired minus Velocity
    PVector steer = desired - velocity;
    steer.limit(maxforce);  // Limit to maximum steering force
    return steer;
  }

  // A method that calculates a steering force towards a target
  // STEER = DESIRED MINUS VELOCITY
  void arrive(PVector target) {
    PVector desired = target - location;  // A vector pointing from the location to the target
    float d = desired.mag();
    // Normalize desired and scale with arbitrary damping within 50 pixels
    desired.normalize();
    if (d < 4) {
      float m = map(d, 0, 50, 0, maxspeed);
      desired *= m;
    }
    else {
      desired *= maxspeed;
    }

    // Steering = Desired minus Velocity
    PVector steer = desired - velocity;
    steer.limit(maxforce);  // Limit to maximum steering force
    applyForce(steer);

  }

  // Wraparound
  void wrapAroundBorders() {
    if (location.x < 0) location.x = DISPLAY_WIDTH - 1;
    if (location.y < 0) location.y = DISPLAY_HEIGHT - 1;
    if (location.x >= DISPLAY_WIDTH) location.x = 0;
    if (location.y >= DISPLAY_HEIGHT) location.y = 0;
  }

  // bool bounceOffBorders() {
  //     bool bounced = false;

  //     if (location.x >= DISPLAY_WIDTH - 1) {
  //         location.x = DISPLAY_WIDTH - 1;
  //         velocity.x *= -1;
  //         bounced = true;
  //     }
  //     else if (location.x < 0) {
  //         location.x = 0;
  //         velocity.x *= -1;
  //         bounced = true;
  //     }

  //     if (location.y >= DISPLAY_HEIGHT - 1) {
  //         location.y = DISPLAY_HEIGHT - 1;
  //         velocity.y *= -1;
  //         bounced = true;
  //     }
  //     else if (location.y < 0) {
  //         location.y = 0;
  //         velocity.y *= -1;
  //         bounced = true;
  //     }
  //     return bounced;
  // }

};
/*******************************************************************************/
// start code 3d cube
/*******************************************************************************/

// variables for 3dcube
float r = 0.2; // z
float p = 0;   // x
float q = 0;   // y
float a1x, a1y, a1z;
float a2x, a2y, a2z;
float a3x, a3y, a3z;
float a4x, a4y, a4z;
float a5x, a5y, a5z;
float a6x, a6y, a6z;
float a7x, a7y, a7z;
float a8x, a8y, a8z;
float b1x, b1y;
float b2x, b2y;
float b3x, b3y;
float b4x, b4y;
float b5x, b5y;
float b6x, b6y;
float b7x, b7y;
float b8x, b8y;

float b1x_old, b1y_old;
float b2x_old, b2y_old;
float b3x_old, b3y_old;
float b4x_old, b4y_old;
float b5x_old, b5y_old;
float b6x_old, b6y_old;
float b7x_old, b7y_old;
float b8x_old, b8y_old;

float scaler = 2;
float x_rot, y_rot, z_rot;

FLASHMEM void InitializeCube(void)
{
  // define the points of the cube
  a1x = 1;
  a1y = 1;
  a1z = 1;
  a2x = 1;
  a2y = 1;
  a2z = -1;
  a3x = -1;
  a3y = 1;
  a3z = -1;
  a4x = -1;
  a4y = 1;
  a4z = 1;
  a5x = 1;
  a5y = -1;
  a5z = 1;
  a6x = 1;
  a6y = -1;
  a6z = -1;
  a7x = -1;
  a7y = -1;
  a7z = -1;
  a8x = -1;
  a8y = -1;
  a8z = 1;

  x_rot = 0.016;
  y_rot = 0.02;
  z_rot = 0.0224;

  return;
}

FLASHMEM void topolar(float a, float b, float* r, float* theta)
{
  *theta = atan2(b, a);
  *r = sqrt(pow(a, 2) + pow(b, 2));
  return;
}

FLASHMEM void torect(float r, float theta, float* a, float* b)
{
  *a = r * cos(theta);
  *b = r * sin(theta);
  return;
}

FLASHMEM void project(float ax, float ay, float az, float* bx, float* by)
{
  // three point perspective
  *bx = ax / (p * ax + q * ay + r * az + 1) * (float)9.4 * scaler + (DISPLAY_WIDTH / 2);
  *by = ay / (p * ax + q * ay + r * az + 1) * (float)9.4 * scaler + (DISPLAY_HEIGHT / 2) - 8;
}

FLASHMEM void rotate(float* a, float* b, float theta)
{
  float r, theta_new;

  topolar(*a, *b, &r, &theta_new);
  theta_new = theta_new + theta;
  torect(r, theta_new, a, b);
  return;
}

FLASHMEM void rotatex(float* ax, float* ay, float* az, float theta)
{
  rotate(ay, az, theta);
  return;
}

FLASHMEM void rotatey(float* ax, float* ay, float* az, float theta)
{
  rotate(az, ax, theta);
  return;
}

FLASHMEM void rotatez(float* ax, float* ay, float* az, float theta)
{
  rotate(ax, ay, theta);
  return;
}

FLASHMEM void RotateCubeX(float angle)
{
  // rotate the points of the the cube around the X axis
  rotatex(&a1x, &a1y, &a1z, angle);
  rotatex(&a2x, &a2y, &a2z, angle);
  rotatex(&a3x, &a3y, &a3z, angle);
  rotatex(&a4x, &a4y, &a4z, angle);
  rotatex(&a5x, &a5y, &a5z, angle);
  rotatex(&a6x, &a6y, &a6z, angle);
  rotatex(&a7x, &a7y, &a7z, angle);
  rotatex(&a8x, &a8y, &a8z, angle);
  return;
}

FLASHMEM void RotateCubeY(float angle)
{
  // rotate the points of the the cube around the Y axis
  rotatey(&a1x, &a1y, &a1z, angle);
  rotatey(&a2x, &a2y, &a2z, angle);
  rotatey(&a3x, &a3y, &a3z, angle);
  rotatey(&a4x, &a4y, &a4z, angle);
  rotatey(&a5x, &a5y, &a5z, angle);
  rotatey(&a6x, &a6y, &a6z, angle);
  rotatey(&a7x, &a7y, &a7z, angle);
  rotatey(&a8x, &a8y, &a8z, angle);
  return;
}

FLASHMEM void RotateCubeZ(float angle)
{
  // rotate the points of the cube around the Z axis
  rotatez(&a1x, &a1y, &a1z, angle);
  rotatez(&a2x, &a2y, &a2z, angle);
  rotatez(&a3x, &a3y, &a3z, angle);
  rotatez(&a4x, &a4y, &a4z, angle);
  rotatez(&a5x, &a5y, &a5z, angle);
  rotatez(&a6x, &a6y, &a6z, angle);
  rotatez(&a7x, &a7y, &a7z, angle);
  rotatez(&a8x, &a8y, &a8z, angle);
  return;
}

FLASHMEM void ProjectCube()
{
  // project the 3 points of the cube into the X,Y plane
  project(a1x, a1y, a1z, &b1x, &b1y);
  project(a2x, a2y, a2z, &b2x, &b2y);
  project(a3x, a3y, a3z, &b3x, &b3y);
  project(a4x, a4y, a4z, &b4x, &b4y);
  project(a5x, a5y, a5z, &b5x, &b5y);
  project(a6x, a6y, a6z, &b6x, &b6y);
  project(a7x, a7y, a7z, &b7x, &b7y);
  project(a8x, a8y, a8z, &b8x, &b8y);
}

FLASHMEM void DrawCube()
{
  int col = ColorHSV(screensaver_counthue, 254, screensaver_brightness);

  display.drawLine(b5x_old, b5y_old, b8x_old, b8y_old, COLOR_BACKGROUND);
  display.drawLine(b5x_old, b5y_old, b6x_old, b6y_old, COLOR_BACKGROUND);
  display.drawLine(b2x_old, b2y_old, b6x_old, b6y_old, COLOR_BACKGROUND);
  display.drawLine(b8x_old, b8y_old, b7x_old, b7y_old, COLOR_BACKGROUND);
  display.drawLine(b2x_old, b2y_old, b3x_old, b3y_old, COLOR_BACKGROUND);
  display.drawLine(b7x_old, b7y_old, b6x_old, b6y_old, COLOR_BACKGROUND);
  display.drawLine(b7x_old, b7y_old, b3x_old, b3y_old, COLOR_BACKGROUND);
  display.drawLine(b1x_old, b1y_old, b5x_old, b5y_old, COLOR_BACKGROUND);
  display.drawLine(b4x_old, b4y_old, b3x_old, b3y_old, COLOR_BACKGROUND);
  display.drawLine(b1x_old, b1y_old, b4x_old, b4y_old, COLOR_BACKGROUND);
  display.drawLine(b4x_old, b4y_old, b8x_old, b8y_old, COLOR_BACKGROUND);
  display.drawLine(b1x_old, b1y_old, b2x_old, b2y_old, COLOR_BACKGROUND);

  display.drawLine(b5x, b5y, b8x, b8y, col);
  display.drawLine(b5x, b5y, b6x, b6y, col);
  display.drawLine(b2x, b2y, b6x, b6y, col);
  display.drawLine(b8x, b8y, b7x, b7y, col);
  display.drawLine(b2x, b2y, b3x, b3y, col);
  display.drawLine(b7x, b7y, b6x, b6y, col);
  display.drawLine(b7x, b7y, b3x, b3y, col);
  display.drawLine(b1x, b1y, b5x, b5y, col);
  display.drawLine(b4x, b4y, b3x, b3y, col);
  display.drawLine(b1x, b1y, b4x, b4y, col);
  display.drawLine(b4x, b4y, b8x, b8y, col);
  display.drawLine(b1x, b1y, b2x, b2y, col);

  b1x_old = b1x;
  b1y_old = b1y;
  b2x_old = b2x;
  b2y_old = b2y;
  b3x_old = b3x;
  b3y_old = b3y;
  b4x_old = b4x;
  b4y_old = b4y;
  b5x_old = b5x;
  b5y_old = b5y;
  b6x_old = b6x;
  b6y_old = b6y;
  b7x_old = b7x;
  b7y_old = b7y;
  b8x_old = b8x;
  b8y_old = b8y;

  return;
}

FLASHMEM void qix_screensaver()
{
  display.drawLine(qix.x0s[qix_num - 1], qix.y0s[qix_num - 1], qix.x1s[qix_num - 1], qix.y1s[qix_num - 1], 0);
  for (uint8_t j = 0; j < qix_num - 1; j++)
  {
    uint16_t hsv = (screensaver_counthue + 2 * j) % 360;
    display.drawLine(qix.x0s[j], qix.y0s[j], qix.x1s[j], qix.y1s[j], ColorHSV(hsv, 254, screensaver_brightness));
  }
  for (uint8_t j = qix_num - 1; j >= 1; j--)
  {
    qix.x0s[j] = qix.x0s[j - 1];
    qix.x1s[j] = qix.x1s[j - 1];
    qix.y0s[j] = qix.y0s[j - 1];
    qix.y1s[j] = qix.y1s[j - 1];
  }
  qix.x0s[0] += qix.dx0;
  qix.x1s[0] += qix.dx1;
  qix.y0s[0] += qix.dy0;
  qix.y1s[0] += qix.dy1;
#define limit(v, dv, max_v)         \
    {                               \
        if (v < 0)                  \
        {                           \
            v = 0;                  \
            dv = (rand() & 6) + 2;  \
        }                           \
        if (v >= max_v)             \
        {                           \
            v = max_v - 1;          \
            dv = -(rand() & 6) - 2; \
        }                           \
    }
  limit(qix.x0s[0], qix.dx0, TFT_HEIGHT);
  limit(qix.x1s[0], qix.dx1, TFT_HEIGHT);
  limit(qix.y0s[0], qix.dy0, TFT_WIDTH);
  limit(qix.y1s[0], qix.dy1, TFT_WIDTH);
#undef limit
}

FLASHMEM void cube_frame()
{
  // rotate the cube points around the defined axis
  RotateCubeX(x_rot);
  RotateCubeY(y_rot);
  RotateCubeZ(z_rot);

  // perform a 2D project of the 3D object onto the X,Y plane
  // with Z perspective
  ProjectCube();
  // Draw the cube on the screen
  DrawCube();
}

FLASHMEM void cube_screensaver()
{
  cube_frame();

  static float time = 0.0f;
  time += 0.05f; // Adjust this value to control animation speed
  
  // Smooth oscillation between 2 and 7 using sine wave
  scaler = 4.5f + 2.5f * sinf(time); // 4.5 ± 2.5 = range [2, 7]
}

/*
copyright 2007 Mike Edwards

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 */

 //using the algorithm from http://freespace.virgin.net/hugo.elias/models/m_perlin.html

float Noise2(float x, float y)
{
  long noise;
  noise = (long)x + (long)y * 57;
  return (1.0 - ((long)(noise * (noise * noise * 15731L + 789221L) + 1376312589L) & 0x7fffffff) / 1073741824.0);
}

float SmoothNoise2(float x, float y)
{
  float corners, sides, center;
  corners = (Noise2(x - 1, y - 1) + Noise2(x + 1, y - 1) + Noise2(x - 1, y + 1) + Noise2(x + 1, y + 1)) / 16;
  sides = (Noise2(x - 1, y) + Noise2(x + 1, y) + Noise2(x, y - 1) + Noise2(x, y + 1)) / 8;
  center = Noise2(x, y) / 4;
  return (corners + sides + center);
}

float CosineInterpolate(float a, float b, float x)
{
  float ft = x * 3.1415927;
  float f = (1 - cos(ft)) * .5;
  return(a * (1 - f) + b * f);
}

float Interpolate(float a, float b, float x)
{
  return(CosineInterpolate(a, b, x));
}
float InterpolatedNoise2(float x, float y)
{
  float v1, v2, v3, v4, i1, i2, fractionX, fractionY;
  long longX, longY;
  longX = (long)(x);
  fractionX = x - longX;
  longY = (long)(y);
  fractionY = y - longY;
  v1 = SmoothNoise2(longX, longY);
  v2 = SmoothNoise2(longX + 1, longY);
  v3 = SmoothNoise2(longX, longY + 1);
  v4 = SmoothNoise2(longX + 1, longY + 1);
  i1 = Interpolate(v1, v2, fractionX);
  i2 = Interpolate(v3, v4, fractionX);
  return(Interpolate(i1, i2, fractionY));
}
float LinearInterpolate(float a, float b, float x)
{
  return(a * (1 - x) + b * x);
}
float  persistence;
float PerlinNoise2(float x, float y, float persistance, int octaves)
{
  float frequency, amplitude;
  float total = 0.0;
  for (int i = 0; i <= octaves - 1; i++)
  {
    frequency = pow(2, i);
    amplitude = pow(persistence, i);
    total = total + InterpolatedNoise2(x * frequency, y * frequency) * amplitude;
  }
  return(total);
}

FLASHMEM void terrain_project(float ax, float ay, float az, float* bx, float* by)
{
  // three point perspective
  *bx = ax / (p * ax + q * ay + r * az + 1) + DISPLAY_WIDTH / 2;
  *by = ay / (p * ax + q * ay + r * az + 1) + 110;

}
using namespace std;
int fly = 1;

class Terrain {
public:
  int cols, rows;
  int scl = 16;
  int xoffset = DISPLAY_WIDTH / 2 + scl * 2;
  int octaves;
  float yy = 3.0;
  float ang = -0.04;
  void setup()
  {
    // display.fillScreen(COLOR_BACKGROUND);
    fly = 1;
    cols = DISPLAY_WIDTH / scl;
    rows = DISPLAY_HEIGHT / scl + 7;
    //persistence affects the degree to which the "finer" noise is seen
    persistence = 0.25;
    //octaves are the number of "layers" of noise that get computed
    octaves = 3;
  }
  void draw(int yOffset)
  {
    float d_x1 = 0;
    float d_x2 = 0;
    float d_y1 = 0;
    float d_y2 = 0;
    int col;
    uint8_t z_shift = 0;
    for (int y = yOffset; y < rows; y++) {
      // darken at horizon a bit
      uint8_t brightness = screensaver_brightness;
      float factor = (y - yOffset) * 0.05;
      brightness *= min(factor, 1.0);

      for (int x = 0; x < cols + 4; x++) {
        for (uint8_t d = 0; d < 2; d++) {
          if (d == 0)
          {
            col = COLOR_BACKGROUND;
            z_shift = 1;
          }
          else
          {
            col = ColorHSV(screensaver_counthue, 254, brightness);
            z_shift = 0;
          }
          a1x = x * scl - xoffset;
          a1y = y * yy;
          a1z = PerlinNoise2((float)x, y - fly + z_shift, persistence, octaves);
          rotatex(&a1x, &a1y, &a1z, ang);
          terrain_project(a1x, a1y, a1z, &b1x, &b1y);
          d_x1 = b1x;
          d_y1 = b1y;
          a1x = (x + 1) * scl - xoffset;
          a1y = y * yy;
          a1z = PerlinNoise2((float)(x + 1), y - fly + z_shift, persistence, octaves);
          rotatex(&a1x, &a1y, &a1z, ang);
          terrain_project(a1x, a1y, a1z, &b1x, &b1y);
          d_x2 = b1x;
          d_y2 = b1y;
          display.drawLine(d_x1, d_y1, d_x2, d_y2, col);
          a1y = y * yy + yy;
          a1z = PerlinNoise2((float)(x + 1), (y + 1 - fly + z_shift), persistence, octaves);
          rotatex(&a1x, &a1y, &a1z, ang);
          terrain_project(a1x, a1y, a1z, &b1x, &b1y);
          d_x1 = b1x;
          d_y1 = b1y;
          display.drawLine(d_x1, d_y1, d_x2, d_y2, col);
        }
      }
      check_midi_devices();
    }
    fly = fly + 1;
  }
};

Terrain terrain;
int yTerrainOffset = 0;

FLASHMEM void terrain_init()
{
  terrain.setup();
  if (rand() & 0x01) {
    yTerrainOffset = 3;
    splash_draw_header();
    splash_draw_D();
    splash_draw_X(1);
    splash_draw_reverseD();
  }
  else {
    yTerrainOffset = 0;
    draw_logo2(20);
  }
}
FLASHMEM void terrain_frame()
{
  terrain.draw(yTerrainOffset);
}

class PatternFlock {
public:

  Boid boids[boidCount];
  Boid predator;
  PVector wind;

  int flock_buffer_x[boidCount];
  int flock_buffer_y[boidCount];

  int predator_buffer_x;
  int predator_buffer_y;

  void start() {
    for (int i = 0; i < boidCount; i++) {
      boids[i] = Boid(random(DISPLAY_WIDTH), random(DISPLAY_HEIGHT));
    }
    predator = Boid(random(DISPLAY_WIDTH), random(DISPLAY_HEIGHT));
    predator.maxforce *= 1.6666666;
    predator.maxspeed *= 1.1;
    predator.neighbordist = 25.0;
    predator.desiredseparation = 0.0;
  }

  unsigned int drawFrame() {
    bool applyWind = random(0, 255) > 250;
    if (applyWind) {
      wind.x = Boid::randomf();
      wind.y = Boid::randomf();
    }

    int col = ColorHSV(0, 0, screensaver_brightness);
    for (int i = 0; i < boidCount; i++) {
      Boid* boid = &boids[i];

      // flee from predator
      boid->repelForce(predator.location, 25);

      boid->run(boids);
      PVector location = boid->location;

      display.fillRect(flock_buffer_x[i], flock_buffer_y[i], 2, 2, COLOR_BACKGROUND);
      display.fillRect(location.x, location.y, 2, 2, col);

      flock_buffer_x[i] = location.x;
      flock_buffer_y[i] = location.y;

      if (applyWind) {
        boid->applyForce(wind);
        applyWind = false;
      }
    }

    predator.run(boids);
    PVector location = predator.location;
    display.fillCircle(predator_buffer_x, predator_buffer_y, 3, COLOR_BACKGROUND);
    display.fillCircle(location.x, location.y, 3, ColorHSV(screensaver_counthue, 255, screensaver_brightness));
    predator_buffer_x = location.x;
    predator_buffer_y = location.y;

    return 50;
  }
};
PatternFlock flock_instance;

FLASHMEM void flock_init()
{
  flock_instance.start();
}
FLASHMEM void flock_frame()
{
  flock_instance.drawFrame();
}

FLASHMEM void boot_animation()
{
  display.setTextSize(2);
  randomSeed(analogRead(0));
  char chars[16 * 10];
  char chars_buffer[16 * 10];
  char text1[10] = { 'W', 'E', 'L', 'C', 'O', 'M', 'E', ' ', 'T', 'O' };
  char text2[10] = { 'M', 'I', 'C', 'R', 'O', 'D', 'E', 'X', 'E', 'D' };
  uint8_t pos_x_1 = random(4);
  uint8_t pos_y_1 = random(4) + 1;
  uint8_t pos_x_2 = random(4) + 3;
  uint8_t pos_y_2 = random(4) + 5;
  uint8_t match_count_1 = 0;
  uint8_t match_count_2 = 0;
  uint8_t count = 0;
  uint8_t hue = random(2);
  uint8_t fade_point = 60;

  for (uint8_t t = 0; t < 16 * 10; t++)
  {
    chars[t] = 32 + random(92 - 32);
  }

  for (uint8_t f = 0; f < 125; f++)
  {
    for (uint8_t x = 0; x < 16; x++)
    {
      for (uint8_t y = 0; y < 10; y++)
      {
        if (y == pos_y_1 && x >= pos_x_1 && x < pos_x_1 + 10 && f < fade_point)
        {
          if (chars[count] > text1[match_count_1])
            chars[count]--;
          else  if (chars[count] < text1[match_count_1])
            chars[count]++;
          match_count_1++;
          display.setTextColor(ColorHSV(hue * 110, f * 3.8, 254), 0);
        }
        else
          if (y == pos_y_2 && x >= pos_x_2 && x < pos_x_2 + 10 && f < fade_point)
          {
            if (chars[count] > text2[match_count_2])
              chars[count]--;
            else  if (chars[count] < text2[match_count_2])
              chars[count]++;
            match_count_2++;
            display.setTextColor(ColorHSV(hue * 110, f * 3.8, 254), 0);
          }
          else
          {
            if ((chars[count] > 32 && random(2) != 0) || (f >= fade_point && chars[count] > 32 && random(7) != 1))
              chars[count]--;

            if ((y == pos_y_1 && x >= pos_x_1 && x < pos_x_1 + 10) || (y == pos_y_2 && x >= pos_x_2 && x < pos_x_2 + 10))
              display.setTextColor(ColorHSV(hue * 110, fade_point * 3.8, 254 + fade_point - (f * 1.9)), 0);
            else
              display.setTextColor(ColorHSV(0, 0, 255 - f * 2), 0);
          }
        if (chars_buffer[count] != chars[count] || y == pos_y_1 || y == pos_y_2)
        {
          display.setCursor(20 + x * 18, 18 + y * 21);
          display.print(chars[count]);
          chars_buffer[count] = chars[count];
        }
        count++;
      }
    }
    count = 0;
    match_count_1 = 0;
    match_count_2 = 0;
    delay(30);
  }
}

class GameOfLife {
private:
  uint16_t col = COLOR_SYSTEXT;
  uint8_t CELL_SIZE = 4; // Adjustable cell size
  uint16_t GRID_WIDTH;
  uint16_t GRID_HEIGHT;
  uint8_t** grid;
  uint8_t** newGrid;

public:
  GameOfLife(uint8_t cellSize = 4) : CELL_SIZE(cellSize) {
    GRID_WIDTH = DISPLAY_WIDTH / CELL_SIZE;
    GRID_HEIGHT = DISPLAY_HEIGHT / CELL_SIZE;

    // Allocate grid memory
    grid = new uint8_t * [GRID_WIDTH];
    newGrid = new uint8_t * [GRID_WIDTH];
    for (uint16_t x = 0; x < GRID_WIDTH; x++) {
      grid[x] = new uint8_t[GRID_HEIGHT];
      newGrid[x] = new uint8_t[GRID_HEIGHT];
    }
  }

  ~GameOfLife() {
    // Free memory
    for (uint16_t x = 0; x < GRID_WIDTH; x++) {
      delete[] grid[x];
      delete[] newGrid[x];
    }
    delete[] grid;
    delete[] newGrid;
  }

  FLASHMEM void initialize() {
    randomSeed(analogRead(0));
    for (uint16_t x = 0; x < GRID_WIDTH; x++) {
      for (uint16_t y = 0; y < GRID_HEIGHT; y++) {
        // Random initialization - about 15% of cells alive
        grid[x][y] = (random(100) < 15) ? 1 : 0;
        drawCell(x, y, grid[x][y]);
      }
    }
  }

  FLASHMEM void update() {
    col = ColorHSV(screensaver_counthue, 254, 254);
    computeNextGeneration();
  }

  FLASHMEM void setCellSize(uint8_t size) {
    if (size < 2) size = 2;
    if (size > 20) size = 20;
    if (size != CELL_SIZE) {
      CELL_SIZE = size;
      resetGridDimensions();
      initialize();
    }
  }

private:
  FLASHMEM void resetGridDimensions() {
    // Free old memory
    for (uint16_t x = 0; x < GRID_WIDTH; x++) {
      delete[] grid[x];
      delete[] newGrid[x];
    }
    delete[] grid;
    delete[] newGrid;

    // Calculate new dimensions
    GRID_WIDTH = DISPLAY_WIDTH / CELL_SIZE;
    GRID_HEIGHT = DISPLAY_HEIGHT / CELL_SIZE;

    // Allocate new memory
    grid = new uint8_t * [GRID_WIDTH];
    newGrid = new uint8_t * [GRID_WIDTH];
    for (uint16_t x = 0; x < GRID_WIDTH; x++) {
      grid[x] = new uint8_t[GRID_HEIGHT];
      newGrid[x] = new uint8_t[GRID_HEIGHT];
    }
  }

   void computeNextGeneration() {
    // Compute next generation
    for (uint16_t x = 0; x < GRID_WIDTH; x++) {
      for (uint16_t y = 0; y < GRID_HEIGHT; y++) {
        uint8_t neighbors = countNeighbors(x, y);

        // Apply Conway's rules
        if (grid[x][y] == 1) {
          newGrid[x][y] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
        }
        else {
          newGrid[x][y] = (neighbors == 3) ? 1 : 0;
        }
      }
    }

    // Copy new generation to current and update display
    for (uint16_t x = 0; x < GRID_WIDTH; x++) {
      for (uint16_t y = 0; y < GRID_HEIGHT; y++) {
        if (grid[x][y] != newGrid[x][y]) {
          grid[x][y] = newGrid[x][y];
          drawCell(x, y, grid[x][y]);
        }
      }
    }
  }

  FLASHMEM uint8_t countNeighbors(uint16_t x, uint16_t y) {
    uint8_t count = 0;
    for (int8_t i = -1; i <= 1; i++) {
      for (int8_t j = -1; j <= 1; j++) {
        if (i == 0 && j == 0) continue;

        int16_t nx = x + i;
        int16_t ny = y + j;

        // Wrap around the edges
        if (nx < 0) nx = GRID_WIDTH - 1;
        if (nx >= GRID_WIDTH) nx = 0;
        if (ny < 0) ny = GRID_HEIGHT - 1;
        if (ny >= GRID_HEIGHT) ny = 0;

        count += grid[nx][ny];
      }
    }
    return count;
  }

  FLASHMEM void drawCell(uint16_t x, uint16_t y, uint8_t state) {
    display.fillRect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE, state ? col : COLOR_BACKGROUND);
  }
};

// Global instance
GameOfLife life;

FLASHMEM void gol_setup() {
  // Initialize your display here

  life.initialize();
}

FLASHMEM void gol_loop() {
  life.update();
  delay(120); // Adjust speed as needed
}

//Connection Machine CM5
// Use bit-packed storage for exact same dimensions: 18 rows x 34 columns
uint32_t ar_bits[6] = { 0 }; // 6x32 bits = 192 bits, enough for 18x34 (612 bits needed)
uint8_t pixel;
uint8_t w = 16;
uint8_t spacer=4;
#define RNUM_SEED 0xBAD
static uint16_t rnum = RNUM_SEED;

static uint16_t get_random_bit(void)
{
#define X rnum
  uint16_t lfsr_bit = ((X >> 0) ^ (X >> 1) ^ (X >> 3) ^ (X >> 12)) & 1;
  uint16_t rand_bit = (X | (X >> 2)) & 1;
#undef X
  rnum = (lfsr_bit << 15) | (rnum >> 1);
  return rand_bit;
}

// Get pixel from bit-packed array 
FLASHMEM bool get_pixel(uint8_t i, uint8_t j) {
  uint16_t index = i * 34 + j;  // 34 columns like original
  uint8_t array_index = index / 32;
  uint8_t bit_index = index % 32;
  return (ar_bits[array_index] >> bit_index) & 1;
}

// Set pixel in bit-packed array 
FLASHMEM void set_pixel(uint8_t i, uint8_t j, bool state) {
  uint16_t index = i * 34 + j;  // 34 columns like original
  uint8_t array_index = index / 32;
  uint8_t bit_index = index % 32;

  if (state) {
    ar_bits[array_index] |= (1UL << bit_index);
  }
  else {
    ar_bits[array_index] &= ~(1UL << bit_index);
  }
}

FLASHMEM void paintCM5_I(int x, int y) {
  uint8_t i, j;
  uint8_t ledsize = 3;
  for (i = 0; i < w; i++) {
    for (j = 0; j < 32; j++) {
      bool pixel = get_pixel(i, j);
      if (j % 2 == 0) {
        if (pixel)
          display.fillRect(((w - i) * spacer) - spacer + x, j * spacer + y, ledsize, ledsize, RED);
        else
          display.fillRect(((w - i) * spacer) - spacer + x, j * spacer + y, ledsize, ledsize, COLOR_BACKGROUND);
      }
      else {
        if (pixel)
          display.fillRect(((i + 1) * spacer) - spacer + x, j * spacer + y, ledsize, ledsize, RED);
        else
          display.fillRect(((i + 1) * spacer) - spacer + x, j * spacer + y, ledsize, ledsize, COLOR_BACKGROUND);
      }
    }
  }
}

FLASHMEM void CM5_init() {
  display.fillScreen(COLOR_BACKGROUND);
  // Clear the bit array
  for (int i = 0; i < 6; i++) {
    ar_bits[i] = 0;
  }
}

FLASHMEM void CM5_frame_I() {
  uint16_t bit;

  for (int j = 32; j >= 0; j--) {
    bit = get_random_bit();
    if (bit == 0) {
      set_pixel(w, j, true);  // ar[w][j] = 1 where w=16
    }
    else {
      set_pixel(w, j, false); // ar[w][j] = 0
    }
  }

  // shifting logic
  for (int j = 0; j < 32; j++) {
    for (int i = 0; i < w + 1; i++) {
      bool pixel_val = get_pixel(i + 1, j);
      set_pixel(i, j, pixel_val);
    }
  }

  // drawing calls
  for (int xx = 0; xx < 3; xx++) {
    paintCM5_I((xx * 32) * spacer, spacer);
    paintCM5_I((xx * 32) * spacer, spacer * 32);
  }

  check_midi_devices();
  delay(100);
}

FLASHMEM void paintCM5_II(int x, int y) {
  uint8_t offset = DISPLAY_WIDTH / 2 - ((spacer * 2) * 9);
  uint8_t i, j;
  uint8_t ledsize = 6;

  for (i = 0; i < w ; i++) {
    for (j = 0; j < 32; j++) {
      bool pixel = get_pixel(i, j);
      if (j % 2 == 0) {
        if (pixel)
          display.fillRect(offset + ((w - i) * spacer * 2) + x, (j * 3) * spacer + y, ledsize, ledsize, RED);
        else
          display.fillRect(offset + ((w - i) * spacer * 2) + x, (j * 3) * spacer + y, ledsize, ledsize, COLOR_BACKGROUND);
      }
      else {
        if (pixel)
          display.fillRect(offset + ((i + 1) * spacer * 2) + x, (j * 3) * spacer + y, ledsize, ledsize, RED);
        else
          display.fillRect(offset + ((i + 1) * spacer * 2) + x, (j * 3) * spacer + y, ledsize, ledsize, COLOR_BACKGROUND);
      }
    }
  }
}

FLASHMEM void CM5_frame_II() {
  uint16_t bit;

  for (int j = 32; j >= 0; j--) {
    bit = get_random_bit();
    if (bit == 0) {
      set_pixel(w, j, true);
    }
    else {
      set_pixel(w, j, false);
    }
  }

  // shifting logic
  for (int j = 0; j < 32; j++) {
    for (int i = 0; i < w + 1; i++) {
      bool pixel_val = get_pixel(i + 1, j);
      set_pixel(i, j, pixel_val);
    }
  }

 paintCM5_II(0, 2);
  delay(150);
  check_midi_devices();

}


#include <math.h>

// External declarations for global variables
extern bool remote_active;
extern void check_remote();
// Assuming display is defined elsewhere with appropriate methods

AsteroidsScreensaver::AsteroidsScreensaver() : score(0), activeAsteroids(0) {}

FLASHMEM void AsteroidsScreensaver::begin() {
  randomSeed(analogRead(0));

  // Initialize spaceship at center
  ship.x = DISPLAY_WIDTH / 2;
  ship.y = DISPLAY_HEIGHT / 2;
  ship.angle = 0;
  ship.size = 8;
  ship.cooldown = 0;
  activeAsteroids = 0;
  ship.vx = 0;
  ship.vy = 0;
  ship.thrusterFuel = 100.0f;
  ship.shield = 100.0f;
  ship.thrusting = false;
  ship.prevThrusting = false;
  ship.wasHit = false;
  score = 0;

  // Initialize all shots as inactive
  for (int i = 0; i < MAX_SHOTS; i++) {
    ship.shots[i].active = false;
  }

  // Initialize all asteroids as inactive
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    asteroids[i].active = false;
  }

  // Spawn starting asteroids off-screen
  for (int i = 0; i < STARTING_ASTEROIDS; i++) {
    spawnAsteroidFromBorder();
  }
}

FLASHMEM void AsteroidsScreensaver::update() {
   // Ensure ship is initialized
  if (ship.x == 0 && ship.y == 0) {
    ship.x = DISPLAY_WIDTH / 2;
    ship.y = DISPLAY_HEIGHT / 2;
    ship.prevX = ship.x;
    ship.prevY = ship.y;
  }
  // Reset asteroid flash states
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].active) {
      asteroids[i].flash = false;
    }
  }

  savePreviousState();
  erasePreviousState();

  if (remote_active) {
    display.flushSysEx();
  }

  // Update asteroids
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].active) {
      updateAsteroid(asteroids[i]);
    }
  }

  updateSpaceship();

  // Draw asteroids with flash effect
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].active) {
      uint16_t color = asteroids[i].flash ? RED : COLOR_SYSTEXT;
      drawAsteroid(asteroids[i], color);
    }
  }

  // Draw ship and shots
  drawSpaceship(0xFFFF);

  for (int i = 0; i < MAX_SHOTS; i++) {
    if (ship.shots[i].active) {
      drawShot(ship.shots[i]);
    }
  }

  // Spawn new asteroids if less than 2 on screen
  if (activeAsteroids < MIN_ASTEROIDS) {
    spawnAsteroidFromBorder();
  }

  // Reset hit flag
  ship.wasHit = false;

  // Draw score at top middle
  drawScore();
  // Draw gauges
  drawFuelGauge();
  drawShieldGauge();

  if (remote_active) {
    display.flushSysEx();
  }
}

FLASHMEM void AsteroidsScreensaver::initAsteroidShape(Asteroid& a) {
  int basePoints = 0;
  float baseSize = 0;
  switch (a.size) {
  case LARGE:
    basePoints = random(8, MAX_POINTS);
    baseSize = 20;
    a.radius = 20;
    break;
  case MEDIUM:
    basePoints = random(6, 9);
    baseSize = 12;
    a.radius = 12;
    break;
  case SMALL:
    basePoints = random(5, 7);
    baseSize = 6;
    a.radius = 6;
    break;
  }

  a.numPoints = basePoints;

  for (int i = 0; i < a.numPoints; i++) {
    float radius = baseSize * (0.7 + random(30) / 100.0);
    float angle = 2 * PI * i / a.numPoints;
    a.relX[i] = cos(angle) * radius;
    a.relY[i] = sin(angle) * radius;
  }

  a.rotation = random(360) * PI / 180.0;
  a.rotationSpeed = random(-5, 5) / 100.0;
}

FLASHMEM void AsteroidsScreensaver::initAsteroid(Asteroid& a, AsteroidSize size) {
  a.size = size;
  a.active = true;

  // Updated hit points based on size
  switch (size) {
  case LARGE: a.hitPoints = 3; break;
  case MEDIUM: a.hitPoints = 2; break;
  case SMALL: a.hitPoints = 1; break;
  }
  a.flash = false;

  float speed = 0;
  switch (size) {
  case LARGE:
    speed = random(MIN_SPEED * 10, MAX_SPEED * 10) / 20.0;
    break;
  case MEDIUM:
    speed = random(MIN_SPEED * 15, MAX_SPEED * 15) / 20.0;
    break;
  case SMALL:
    speed = random(MIN_SPEED * 20, MAX_SPEED * 20) / 20.0;
    break;
  }

  a.centerX = random(DISPLAY_WIDTH);
  a.centerY = random(DISPLAY_HEIGHT);

  float angle = random(360) * PI / 180.0;
  a.vx = cos(angle) * speed;
  a.vy = sin(angle) * speed;

  initAsteroidShape(a);
}

FLASHMEM void AsteroidsScreensaver::updateAsteroid(Asteroid& a) {
  if (!a.active) return;

  a.prevCenterX = a.centerX;
  a.prevCenterY = a.centerY;

  // Update position with speed limits
  a.centerX += a.vx;
  a.centerY += a.vy;

  // Constrain speed to halved MAX_SPEED
  float speed = sqrt(a.vx * a.vx + a.vy * a.vy);
  if (speed > MAX_SPEED) {
    a.vx = (a.vx / speed) * MAX_SPEED;
    a.vy = (a.vy / speed) * MAX_SPEED;
  }

  // Screen wrapping
  if (a.centerX < -50) a.centerX = DISPLAY_WIDTH + 50;
  if (a.centerX > DISPLAY_WIDTH + 50) a.centerX = -50;
  if (a.centerY < -50) a.centerY = DISPLAY_HEIGHT + 50;
  if (a.centerY > DISPLAY_HEIGHT + 50) a.centerY = -50;

  // Update rotation with speed limit
  a.rotation += constrain(a.rotationSpeed, -0.1f, 0.1f);
}

FLASHMEM void AsteroidsScreensaver::drawAsteroid(Asteroid& a, uint16_t color) {
  if (!a.active) return;

  for (int i = 0; i < a.numPoints; i++) {
    int next = (i + 1) % a.numPoints;

    float cosRot = cos(a.rotation);
    float sinRot = sin(a.rotation);

    int x0 = a.centerX + (a.relX[i] * cosRot - a.relY[i] * sinRot);
    int y0 = a.centerY + (a.relX[i] * sinRot + a.relY[i] * cosRot);
    int x1 = a.centerX + (a.relX[next] * cosRot - a.relY[next] * sinRot);
    int y1 = a.centerY + (a.relX[next] * sinRot + a.relY[next] * cosRot);

    check_remote();

    display.drawLine(x0, y0, x1, y1, color);
  }
}

FLASHMEM void AsteroidsScreensaver::spawnAsteroidFromBorder() {
  int slot = -1;
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!asteroids[i].active) {
      slot = i;
      break;
    }
  }
  if (slot == -1) return;

  // Choose a random border (0=top, 1=right, 2=bottom, 3=left)
  int border = random(4);
  float startX = 0, startY = 0;

  // Set starting position just outside the screen
  switch (border) {
  case 0: // Top
    startX = random(DISPLAY_WIDTH);
    startY = -20;
    break;
  case 1: // Right
    startX = DISPLAY_WIDTH + 20;
    startY = random(DISPLAY_HEIGHT);
    break;
  case 2: // Bottom
    startX = random(DISPLAY_WIDTH);
    startY = DISPLAY_HEIGHT + 20;
    break;
  case 3: // Left
    startX = -20;
    startY = random(DISPLAY_HEIGHT);
    break;
  }

  // 50% chance to target near ship, 50% chance to target center
  float targetX, targetY;
  if (random(100) < 50) {
    // Target near ship (within 50 pixels)
    targetX = ship.x + random(-50, 50);
    targetY = ship.y + random(-50, 50);
  }
  else {
    // Target center
    targetX = DISPLAY_WIDTH / 2;
    targetY = DISPLAY_HEIGHT / 2;
  }

  // Calculate direction to target
  float dx = targetX - startX;
  float dy = targetY - startY;
  float distance = sqrt(dx * dx + dy * dy);
  float speed = random(5, 10) / 10.0f; // Slower speed for smooth entry

  // Random size: LARGE or MEDIUM
  AsteroidSize size = LARGE;
  int sizeRand = random(2);
  switch (sizeRand) {
  case 0: size = LARGE; break;
  case 1: size = MEDIUM; break;
  }

  asteroids[slot].centerX = startX;
  asteroids[slot].centerY = startY;
  asteroids[slot].vx = (dx / distance) * speed;
  asteroids[slot].vy = (dy / distance) * speed;
  asteroids[slot].size = size;
  asteroids[slot].active = true;

  // Set hit points based on size
  switch (size) {
  case LARGE: asteroids[slot].hitPoints = 3; break;
  case MEDIUM: asteroids[slot].hitPoints = 2; break;
  case SMALL: asteroids[slot].hitPoints = 1; break;
  }

  asteroids[slot].flash = false;

  initAsteroidShape(asteroids[slot]);
  activeAsteroids++;
}

FLASHMEM void AsteroidsScreensaver::updateSpaceship() {
  const float ROTATION_SPEED = 0.03f;  // Half rotation speed
  const float SHOT_SPEED = 12.0f;     // Laser speed

  // Danger detection - avoid nearby asteroids
  bool dangerMode = false;
  int dangerIndex = -1;
  float minDistance = 1000.0f;
  const float DANGER_RADIUS = 80.0f;

  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!asteroids[i].active) continue;

    float dx = asteroids[i].centerX - ship.x;
    float dy = asteroids[i].centerY - ship.y;
    float distance = sqrt(dx * dx + dy * dy);

    if (distance < DANGER_RADIUS && distance < minDistance) {
      dangerMode = true;
      dangerIndex = i;
      minDistance = distance;
    }
  }

  int bestTarget = -1;
  float bestScore = -1000000;  // Start with very low score

  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!asteroids[i].active) continue;

    // Calculate vector to asteroid
    float dx = asteroids[i].centerX - ship.x;
    float dy = asteroids[i].centerY - ship.y;
    float distance = sqrt(dx * dx + dy * dy);

    // Calculate time for laser to reach asteroid
    float timeToHit = distance / SHOT_SPEED;

    // Predict asteroid position at time of hit
    float predictedX = asteroids[i].centerX + asteroids[i].vx * timeToHit;
    float predictedY = asteroids[i].centerY + asteroids[i].vy * timeToHit;

    // Calculate vector to predicted position
    float pdx = predictedX - ship.x;
    float pdy = predictedY - ship.y;
    //float predictedDistance = sqrt(pdx*pdx + pdy*pdy);

    // Calculate angle to predicted position
    float targetAngle = atan2(pdy, pdx);
    float angleDiff = fabs(targetAngle - ship.angle);

    // Normalize angle difference
    while (angleDiff > PI) angleDiff = 2 * PI - angleDiff;

    // Calculate alignment score (higher is better)
    float alignment = cos(angleDiff);
    float sizeFactor = 0.0f;
    switch (asteroids[i].size) {
    case LARGE: sizeFactor = 1.0f; break;
    case MEDIUM: sizeFactor = 0.7f; break;
    case SMALL: sizeFactor = 0.4f; break;
    }

    // Prioritize proximity more heavily
    float proximityFactor = 1.0f + (1.0f - (distance / 300.0f));

    // Score based on alignment, size, and proximity
    float score = (alignment * sizeFactor) * proximityFactor;

    if (score > bestScore) {
      bestScore = score;
      bestTarget = i;
    }
  }

  // Handle danger avoidance
  if (dangerMode) {
    Asteroid& dangerAsteroid = asteroids[dangerIndex];
    float dx = ship.x - dangerAsteroid.centerX;
    float dy = ship.y - dangerAsteroid.centerY;
    float escapeAngle = atan2(dy, dx);

    float angleDiff = escapeAngle - ship.angle;
    if (angleDiff < -PI) angleDiff += 2 * PI;
    if (angleDiff > PI) angleDiff -= 2 * PI;

    // Rotate toward escape vector
    if (angleDiff > ROTATION_SPEED) {
      ship.angle += ROTATION_SPEED;
    }
    else if (angleDiff < -ROTATION_SPEED) {
      ship.angle -= ROTATION_SPEED;
    }
    else {
      ship.angle = escapeAngle;
    }

    // Use thrusters to escape if fuel available and somewhat aligned
    if (fabs(angleDiff) < PI / 2 && ship.thrusterFuel > 20.0f) {
      ship.thrusting = true;
    }
  }
  // Rotate toward best target if found
  else if (bestTarget != -1) {
    // Get asteroid reference
    Asteroid& target = asteroids[bestTarget];

    // Calculate vector to asteroid
    float dx = target.centerX - ship.x;
    float dy = target.centerY - ship.y;
    float distance = sqrt(dx * dx + dy * dy);

    // Calculate time for laser to reach asteroid
    float timeToHit = distance / SHOT_SPEED;

    // Predict asteroid position at time of hit
    float predictedX = target.centerX + target.vx * timeToHit;
    float predictedY = target.centerY + target.vy * timeToHit;

    // Calculate vector to predicted position
    dx = predictedX - ship.x;
    dy = predictedY - ship.y;
    float targetAngle = atan2(dy, dx);

    float angleDiff = targetAngle - ship.angle;
    if (angleDiff < -PI) angleDiff += 2 * PI;
    if (angleDiff > PI) angleDiff -= 2 * PI;

    // Rotate toward target with half speed
    if (angleDiff > ROTATION_SPEED) {
      ship.angle += ROTATION_SPEED;
    }
    else if (angleDiff < -ROTATION_SPEED) {
      ship.angle -= ROTATION_SPEED;
    }
    else {
      ship.angle = targetAngle;
    }
  }

  // Shooting logic
  if (ship.cooldown > 0) ship.cooldown--;

  // Only shoot if we have a target and are properly aligned
  if (!ship.shotActive && ship.cooldown == 0 && bestTarget != -1 && !dangerMode) {
    // Get asteroid reference
    Asteroid& target = asteroids[bestTarget];

    // Calculate vector to asteroid
    float dx = target.centerX - ship.x;
    float dy = target.centerY - ship.y;
    float distance = sqrt(dx * dx + dy * dy);

    // Calculate time for laser to reach asteroid
    float timeToHit = distance / SHOT_SPEED;

    // Predict asteroid position at time of hit
    float predictedX = target.centerX + target.vx * timeToHit;
    float predictedY = target.centerY + target.vy * timeToHit;

    // Calculate vector to predicted position
    dx = predictedX - ship.x;
    dy = predictedY - ship.y;
    float targetAngle = atan2(dy, dx);
    float angleDiff = fabs(targetAngle - ship.angle);

    // Normalize angle difference
    while (angleDiff > PI) angleDiff = 2 * PI - angleDiff;

    // Only shoot if aligned within 5 degrees AND asteroid is at least half on-screen
    if (angleDiff < (5.0 * PI / 180.0) &&
      target.centerX >= target.radius &&
      target.centerX <= DISPLAY_WIDTH - target.radius &&
      target.centerY >= target.radius &&
      target.centerY <= DISPLAY_HEIGHT - target.radius) {

      fireShot();
    }
  }

  // Thruster physics
  if (ship.thrusting && ship.thrusterFuel > 0) {
    // Apply thrust in facing direction
    ship.vx += cos(ship.angle) * THRUST_POWER;
    ship.vy += sin(ship.angle) * THRUST_POWER;

    // Consume fuel
    ship.thrusterFuel -= FUEL_CONSUMPTION;
    if (ship.thrusterFuel < 0) ship.thrusterFuel = 0;
  }
  else {
    ship.thrusting = false;
  }

  // Apply friction (natural deceleration)
  ship.vx *= 0.98f;
  ship.vy *= 0.98f;

  // Maintain minimum drift speed when no danger
  float speed = sqrt(ship.vx * ship.vx + ship.vy * ship.vy);
  const float MIN_DRIFT = 0.2f;  // Very slow base drift speed

  if (!dangerMode && speed < MIN_DRIFT && speed > 0) {
    // Maintain vector direction but at minimum drift speed
    ship.vx = (ship.vx / speed) * MIN_DRIFT;
    ship.vy = (ship.vy / speed) * MIN_DRIFT;
    speed = MIN_DRIFT;
  }

  // Constrain maximum speed
  if (speed > MAX_SHIP_SPEED) {
    ship.vx = (ship.vx / speed) * MAX_SHIP_SPEED;
    ship.vy = (ship.vy / speed) * MAX_SHIP_SPEED;
  }

  // Update ship position
  ship.x += ship.vx;
  ship.y += ship.vy;

  // Screen wrapping for ship
  if (ship.x < 0) ship.x = DISPLAY_WIDTH;
  if (ship.x > DISPLAY_WIDTH) ship.x = 0;
  if (ship.y < 0) ship.y = DISPLAY_HEIGHT;
  if (ship.y > DISPLAY_HEIGHT) ship.y = 0;

  // Recharge thrusters when not in use
  if (!ship.thrusting && ship.thrusterFuel < 100.0f) {
    ship.thrusterFuel += FUEL_CONSUMPTION / 2.0f;
    if (ship.thrusterFuel > 100.0f) ship.thrusterFuel = 100.0f;
  }

  // Recharge shield when not hit
  if (!ship.wasHit && ship.shield < 100.0f) {
    ship.shield += SHIELD_RECHARGE_RATE;
    if (ship.shield > 100.0f) ship.shield = 100.0f;
  }

  // Update laser shot
  for (int s = 0; s < MAX_SHOTS; s++) {
    if (ship.shots[s].active) {
      // Save previous position for drawing
      // int prevX = ship.shots[s].x;
      // int prevY = ship.shots[s].y;

      // Update position
      ship.shots[s].x += ship.shots[s].vx;
      ship.shots[s].y += ship.shots[s].vy;

      // Deactivate if out of bounds
      if (ship.shots[s].x < 0 || ship.shots[s].x >= DISPLAY_WIDTH ||
        ship.shots[s].y < 0 || ship.shots[s].y >= DISPLAY_HEIGHT) {
        ship.shots[s].active = false;
      }
      else {
        // Check collision with asteroids
        for (int i = 0; i < MAX_ASTEROIDS; i++) {
          if (asteroids[i].active) {
            float dx = ship.shots[s].x - asteroids[i].centerX;
            float dy = ship.shots[s].y - asteroids[i].centerY;
            float distance = sqrt(dx * dx + dy * dy);

            if (distance < asteroids[i].radius + 2) {
              asteroids[i].hitPoints--;
              asteroids[i].flash = true;

              if (asteroids[i].hitPoints <= 0) {
                splitAsteroid(i);
              }

              ship.shots[s].active = false;
              break;
            }
          }
        }
      }
    }
  }

  // Check ship collision with asteroids
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].active) {
      float dx = ship.x - asteroids[i].centerX;
      float dy = ship.y - asteroids[i].centerY;
      float distance = sqrt(dx * dx + dy * dy);

      if (distance < asteroids[i].radius + ship.size) {
        if (ship.shield > 0) {
          // Shield absorbs damage
          ship.shield -= SHIELD_DAMAGE;
          if (ship.shield < 0) ship.shield = 0;
          ship.wasHit = true;

          // Flash ship to indicate hit
          drawSpaceship(0xF800); // Red flash
        }
        else {
          // Reset ship to center
          ship.x = DISPLAY_WIDTH / 2;
          ship.y = DISPLAY_HEIGHT / 2;
          ship.vx = 0;
          ship.vy = 0;
          ship.thrusting = false;
          ship.prevX = ship.x;
          ship.prevY = ship.y;
          ship.shield = 100.0f; // Reset shield
        }

        // Split the asteroid that hit
        splitAsteroid(i);
        break;
      }
    }
  }
}

FLASHMEM void AsteroidsScreensaver::splitAsteroid(int index) {
  Asteroid& parent = asteroids[index];
  AsteroidSize newSize = MEDIUM;
  int numFragments = 0;
  int pointsEarned = 0;

  switch (parent.size) {
  case LARGE:
    newSize = MEDIUM;
    numFragments = 2;
    pointsEarned = LARGE_ASTEROID_SCORE;
    break;
  case MEDIUM:
    newSize = SMALL;
    numFragments = 2 + random(1); // 2 or 3
    pointsEarned = MEDIUM_ASTEROID_SCORE;
    break;
  case SMALL:
    // Small asteroids just disappear
    parent.active = false;
    activeAsteroids--;
    pointsEarned = SMALL_ASTEROID_SCORE;
    // Add score for destroying small asteroid
    score += pointsEarned;
    return;
  }

  // Add score for destroying asteroid
  score += pointsEarned;

  // Find inactive asteroids for fragments
  int fragmentSlots[3];
  int foundFragments = 0;

  for (int i = 0; i < MAX_ASTEROIDS && foundFragments < numFragments; i++) {
    if (!asteroids[i].active) {
      fragmentSlots[foundFragments] = i;
      foundFragments++;
    }
  }

  // Create fragments
  if (foundFragments > 0) {
    for (int f = 0; f < foundFragments; f++) {
      int slot = fragmentSlots[f];
      Asteroid& fragment = asteroids[slot];

      // Calculate direction (evenly spaced around circle)
      float angle = f * 2.0f * PI / numFragments;

      // Position offset to prevent overlap
      float offsetDist = parent.radius + 5;
      float offsetX = cos(angle) * offsetDist;
      float offsetY = sin(angle) * offsetDist;

      // Velocity = parent velocity + perpendicular direction
      float speedVariation = random(5, 15) / 10.0f;
      float perpAngle = angle + PI / 2;
      float perpX = cos(perpAngle) * speedVariation;
      float perpY = sin(perpAngle) * speedVariation;

      // Initialize fragment
      fragment.centerX = parent.centerX + offsetX;
      fragment.centerY = parent.centerY + offsetY;
      fragment.vx = parent.vx * 0.7f + perpX;
      fragment.vy = parent.vy * 0.7f + perpY;
      fragment.size = newSize;
      fragment.active = true;

      // Set hit points
      switch (newSize) {
      case SMALL: fragment.hitPoints = 1; break;
      case MEDIUM: fragment.hitPoints = 3; break;
      case LARGE:  fragment.hitPoints = 4;  break;
      }

      // Constrain fragment speed to halved MAX_SPEED
      float speed = sqrt(fragment.vx * fragment.vx + fragment.vy * fragment.vy);
      if (speed > MAX_SPEED) {
        fragment.vx = (fragment.vx / speed) * MAX_SPEED;
        fragment.vy = (fragment.vy / speed) * MAX_SPEED;
      }
      // Initialize shape
      initAsteroidShape(fragment);
    }

    activeAsteroids += foundFragments;
  }

  // Deactivate parent asteroid
  parent.active = false;
  activeAsteroids--;
  ship.shotActive = false;
}

FLASHMEM void AsteroidsScreensaver::fireShot() {
  // Find an inactive shot slot
  int slot = -1;
  for (int i = 0; i < MAX_SHOTS; i++) {
    if (!ship.shots[i].active) {
      slot = i;
      break;
    }
  }
  // If no slot available, don't fire
  if (slot == -1) return;

  // Calculate shot offset to spread shots
  float offsetAngle = 0;
  if (slot == 1) offsetAngle = 0.1f;  // 5.7 degrees
  if (slot == 2) offsetAngle = -0.1f; // 5.7 degrees

  // Activate shot
  ship.shots[slot].x = ship.x + cos(ship.angle + offsetAngle) * ship.size * 1.5;
  ship.shots[slot].y = ship.y + sin(ship.angle + offsetAngle) * ship.size * 1.5;
  ship.shots[slot].vx = cos(ship.angle + offsetAngle) * 12;
  ship.shots[slot].vy = sin(ship.angle + offsetAngle) * 12;
  ship.shots[slot].active = true;

  // Set cooldown for burst control
  ship.cooldown = 10;  // Short cooldown between shots in burst
}

FLASHMEM void AsteroidsScreensaver::drawSpaceship(uint16_t color) {
  // Calculate ship points
  float noseX = ship.x + cos(ship.angle) * ship.size;
  float noseY = ship.y + sin(ship.angle) * ship.size;

  float leftX = ship.x + cos(ship.angle + 2.5 * PI / 3) * ship.size;
  float leftY = ship.y + sin(ship.angle + 2.5 * PI / 3) * ship.size;

  float rightX = ship.x + cos(ship.angle - 2.5 * PI / 3) * ship.size;
  float rightY = ship.y + sin(ship.angle - 2.5 * PI / 3) * ship.size;

  // Draw ship
  check_remote();
  display.drawLine(noseX, noseY, leftX, leftY, color);

  check_remote();
  display.drawLine(noseX, noseY, rightX, rightY, color);

  check_remote();
  display.drawLine(leftX, leftY, ship.x, ship.y, color);

  check_remote();
  display.drawLine(rightX, rightY, ship.x, ship.y, color);

  // Draw thrusters if active
  if (ship.thrusting) {
    // Calculate rear center point
    float rearX = (leftX + rightX) / 2;
    float rearY = (leftY + rightY) / 2;

    // Calculate flame end point
    float flameLength = 7.5f; // Fixed length for consistent erasure
    float flameX = rearX - cos(ship.angle) * flameLength;
    float flameY = rearY - sin(ship.angle) * flameLength;

    // Draw flame with appropriate color
    uint16_t flameColor = (color == 0x0000) ? COLOR_BACKGROUND : RED;

    check_remote();
    display.drawLine(rearX, rearY, flameX, flameY, flameColor);
  }
}

FLASHMEM void AsteroidsScreensaver::drawShot(LaserShot& shot) {
  // Draw a line segment (double length)
  float backX = shot.x - shot.vx * 0.6f;
  float backY = shot.y - shot.vy * 0.6f;

  check_remote();
  display.drawLine(backX, backY, shot.x, shot.y, RED);
}

FLASHMEM void AsteroidsScreensaver::drawFuelGauge() {
  // Double the gauge size
  const uint8_t gaugeWidth = 60;
  const uint8_t gaugeHeight = 10;
  const uint8_t gaugeX = 5;
  const uint8_t gaugeY = 5;


  // Calculate fill width based on fuel
  int fillWidth = (ship.thrusterFuel / 100.0f) * (gaugeWidth - 2);

  if (fillWidth > 0) {
    // Choose color based on fuel level
    uint16_t gaugeColor;
    if (ship.thrusterFuel > 70) gaugeColor = GREEN;
    else if (ship.thrusterFuel > 30) gaugeColor = YELLOW;
    else gaugeColor = RED;

    // Draw filled portion
    check_remote();
    display.fillRect(gaugeX + 1, gaugeY + 1, fillWidth, gaugeHeight - 2, gaugeColor);

    check_remote();
    display.fillRect(gaugeX + 1 + fillWidth, gaugeY + 1, gaugeWidth - fillWidth - 1, gaugeHeight - 2, COLOR_BACKGROUND);

  }

  check_remote();
  // Draw gauge outline
  display.drawRect(gaugeX, gaugeY, gaugeWidth, gaugeHeight, COLOR_SYSTEXT);
}

FLASHMEM void AsteroidsScreensaver::drawShieldGauge() {
  // Shield gauge - same size as fuel gauge
  const uint8_t gaugeWidth = 60;
  const uint8_t gaugeHeight = 10;
  const uint8_t gaugeX = DISPLAY_WIDTH - gaugeWidth - 5; // Top right
  const uint8_t gaugeY = 5;

  // Calculate fill width based on shield
  int fillWidth = (ship.shield / 100.0f) * (gaugeWidth - 2);

  if (fillWidth > 0) {
    // Choose color based on shield level
    uint16_t gaugeColor;
    if (ship.shield > 70) gaugeColor = GREEN;
    else if (ship.shield > 30) gaugeColor = YELLOW;
    else gaugeColor = RED;

    // Draw filled portion
    check_remote();
    display.fillRect(gaugeX + 1, gaugeY + 1, fillWidth, gaugeHeight - 2, gaugeColor);

    check_remote();
    display.fillRect(gaugeX + 1 + fillWidth, gaugeY + 1, gaugeWidth - fillWidth - 1, gaugeHeight - 2, COLOR_BACKGROUND);

  }

  // Draw gauge outline
  if (remote_active) {
    display.console = true;
  }
  display.drawRect(gaugeX, gaugeY, gaugeWidth, gaugeHeight, COLOR_SYSTEXT);

}

FLASHMEM void AsteroidsScreensaver::drawScore() {
  // Display score at top middle (y=5, x=center)
  const int x = 7 * CHAR_width; // Center the score display

  char scoreStr[16];
  display.setTextColor(COLOR_SYSTEXT, COLOR_BACKGROUND);
  display.setTextSize(2);
  snprintf(scoreStr, sizeof(scoreStr), "SCORE: %06d", score);
  // Display with white color
  display.setCursor(x, 0);
  display.print(scoreStr);
}

FLASHMEM void AsteroidsScreensaver::savePreviousState() {
  ship.prevX = ship.x;
  ship.prevY = ship.y;
  ship.prevAngle = ship.angle;
  ship.prevThrusting = ship.thrusting; // Save thruster state

  // Save shot states
  for (int i = 0; i < MAX_SHOTS; i++) {
    if (ship.shots[i].active) {
      ship.shots[i].prevX = ship.shots[i].x;
      ship.shots[i].prevY = ship.shots[i].y;
    }
  }

  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].active) {
      asteroids[i].prevCenterX = asteroids[i].centerX;
      asteroids[i].prevCenterY = asteroids[i].centerY;
      asteroids[i].prevRotation = asteroids[i].rotation;
    }
  }
}

FLASHMEM void AsteroidsScreensaver::erasePreviousState() {
  // Erase asteroids at previous positions
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].active) {
      // Save current state
      float tempX = asteroids[i].centerX;
      float tempY = asteroids[i].centerY;
      float tempRot = asteroids[i].rotation;

      // Use previous state for erasure
      asteroids[i].centerX = asteroids[i].prevCenterX;
      asteroids[i].centerY = asteroids[i].prevCenterY;
      asteroids[i].rotation = asteroids[i].prevRotation;

      drawAsteroid(asteroids[i], COLOR_BACKGROUND);

      // Restore current state
      asteroids[i].centerX = tempX;
      asteroids[i].centerY = tempY;
      asteroids[i].rotation = tempRot;
    }
  }

  // Erase ship at previous state
  float tempX = ship.x;
  float tempY = ship.y;
  float tempAngle = ship.angle;
  bool tempThrusting = ship.thrusting;

  ship.x = ship.prevX;
  ship.y = ship.prevY;
  ship.angle = ship.prevAngle;
  ship.thrusting = ship.prevThrusting; // Use previous thruster state

  drawSpaceship(COLOR_BACKGROUND); // Erase entire ship including thruster

  // Restore current state
  ship.x = tempX;
  ship.y = tempY;
  ship.angle = tempAngle;
  ship.thrusting = tempThrusting;

  // Erase shots using previous state
  for (int i = 0; i < MAX_SHOTS; i++) {
    if (ship.shots[i].active) {
      // Calculate the previous start position for double length
      float prevBackX = ship.shots[i].prevX - ship.shots[i].vx * 0.6f;
      float prevBackY = ship.shots[i].prevY - ship.shots[i].vy * 0.6f;

      // Erase the previous shot segment
      check_remote();
      display.drawLine(prevBackX, prevBackY, ship.shots[i].prevX, ship.shots[i].prevY, 0x0000);
    }
  }
}

AsteroidsScreensaver asteroidsSaver;
