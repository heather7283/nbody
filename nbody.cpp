#include <SFML/Graphics.hpp>
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cmath>
#include <random>

const float pi = std::acos(-1);


sf::Vector2f normalize(const sf::Vector2f& vector) {
  float length = std::sqrt(vector.x * vector.x + vector.y * vector.y);
  if (length == 0) {
    return sf::Vector2f(0, 0);
  }

  return sf::Vector2f(vector.x / length, vector.y / length);
}


class Body {
public:
  unsigned int id;
  float radius;
  float mass;
  sf::Vector2f position;
  sf::Vector2f velocity;
  bool is_collided = false;
  sf::CircleShape shape;

  Body(sf::Vector2f position, float mass, sf::Vector2f velocity = sf::Vector2f(0, 0)) {
    this->id = ++highest_id;
    this->mass = mass;
    this->position = position;
    this->velocity = velocity;
    
    this->radius = std::cbrt(this->mass);

    this->shape = sf::CircleShape(this->radius);
    this->shape.setPosition(this->position);
  }
  
  void draw(sf::RenderWindow* window) {
    this->shape.setPosition(this->position - sf::Vector2f(this->radius, this->radius));
    window->draw(this->shape);
  }

  float get_distance_squared(Body* other) {
    float dx = this->position.x - other->position.x;
    float dy = this->position.y - other->position.y;
    float distance_squared = dx * dx + dy * dy;

    return distance_squared;
  }

  void do_gravity(Body* other) {
    // Play with it to see what fits best
    constexpr float G = .001;
    
    float dx = this->position.x - other->position.x;
    float dy = this->position.y - other->position.y;
    float distance_squared = dx * dx + dy * dy;

    float force = G * this->mass * other->mass / distance_squared;
    
    float distance = std::sqrt(distance_squared);
    sf::Vector2f direction = sf::Vector2f(dx / distance, dy / distance);
    
    // Apply forces
    this->velocity -= direction * force / this->mass;
    other->velocity += direction * force / other->mass;
  }

  bool check_for_collision(Body* other) {
    if (get_distance_squared(other) <= ((this->radius + other->radius) * (this->radius + other->radius))) {
      return true;
    }
    return false;
  }

  void move() {
    this->position += this->velocity;
  }

private:
  static unsigned int highest_id;
};
unsigned int Body::highest_id = 0;


class Camera {
public:
  Body* tracked_body;

  Camera(Body* tracked_body) {
    this->tracked_body = tracked_body;
  }
  
  void apply_offset(std::vector<Body*>& bodies_array, sf::RenderWindow& window) {
    // If we change window size mid-operation coordinates will be messed up
    // so we precalculate window center coordinates
    sf::Vector2f window_center = sf::Vector2f(window.getSize().x / 2, window.getSize().y / 2);
    sf::Vector2f offset = this->tracked_body->position;

    for (int i = 0; i < bodies_array.size(); i++) {
      bodies_array[i]->position += window_center - offset;
    }
  }
};


void generate_random_bodies(int n, std::vector<Body*>* bodies) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> rnd_coords(50.0, 750.0);
  std::uniform_real_distribution<> rnd_masses(1.0, 8.0);
  
  for (int i = 0; i < n; i++) {
    float x = rnd_coords(gen);
    float y = rnd_coords(gen);
    float mass = rnd_masses(gen);

    bodies->push_back(new Body(sf::Vector2f(x, y), mass));
  }
}


void complicated_random_bodies(int n, std::vector<Body*>* bodies, int min_range, int max_range, int min_mass, int max_mass, float speed) {
  // Generate ring of bodies rotating clockwise
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> rnd_r(min_range, max_range);
  std::uniform_real_distribution<> rnd_fi(0, pi * 2);
  std::uniform_real_distribution<> rnd_m(min_mass, max_mass);
  
  for (int i = 0; i < n; i++) {
    float r = rnd_r(gen);
    float fi = rnd_fi(gen);
    float x = std::cos(fi) * r;
    float y = std::sin(fi) * r * -1;

    float r_percent = (r - min_range) / (max_range - min_range) + 0.5f;
    float vel_x = std::sin(fi) * std::sqrt(speed * r_percent);
    float vel_y = std::cos(fi) * std::sqrt(speed * r_percent);

    Body* body = new Body(sf::Vector2f(x, y), rnd_m(gen), sf::Vector2f(vel_x, vel_y));

    bodies->push_back(body);
  }
}


int main() {
  // Create a window
  sf::RenderWindow window(sf::VideoMode(800, 600), "nbody");
  sf::Clock clock;
  
  // Calculate fps
  const float fps = 120.0f;
  sf::Time spf = sf::seconds(1.0f / fps);

  // Array of bodies
  std::vector<Body*> bodies;
  
  complicated_random_bodies(1000, &bodies, 100, 500, 1, 2, 0.01f);
  bodies.push_back(new Body(sf::Vector2f(0, 0), 10000));
  //bodies = {
  //  new Body(sf::Vector2f(100, 100), 64),
  //  new Body(sf::Vector2f(200, 100), 64),
  //  new Body(sf::Vector2f(100, 300), 64),
  //  new Body(sf::Vector2f(200, 300), 64),
  //};
  
  Camera camera(bodies[bodies.size() - 1]);

  std::vector<Body*> new_bodies;
  std::vector<std::pair<Body*, Body*>> collision_groups;
  
  // Main loop
  while (window.isOpen()) {
    // Process events
    sf::Event event;
    while (window.pollEvent(event)) {
      switch (event.type) {
        case sf::Event::Closed:
          window.close();
          break;
        case sf::Event::Resized:
          sf::FloatRect visible_area(0.f, 0.f, event.size.width, event.size.height);
          window.setView(sf::View(visible_area));
          break;
      }
    }
    
    new_bodies.clear();
    collision_groups.clear();
    
    // Process interactions between all unique pairs of bodies
    // NOTE: last body is NOT being processed in the outer loop;
    //       handle it separately
    for (size_t i = 0; i < bodies.size() - 1; i++) {
      Body* first = bodies[i];
      for (size_t j = i + 1; j < bodies.size(); j++) {
        Body* second = bodies[j];
        
        // If first and second collided:
        if (!first->is_collided && !second->is_collided && first->check_for_collision(second)) {
          first->is_collided = true;
          second->is_collided = true;

          collision_groups.push_back(std::pair<Body*, Body*>(first, second));
        }
        
        // Apply gravitational forces to both bodies
        first->do_gravity(second);
      }

      if (!first->is_collided) {
        new_bodies.push_back(first);
      }
    }
    if (!bodies.back()->is_collided) {
      new_bodies.push_back(bodies.back());
    }

    for (size_t i = 0; i < collision_groups.size(); i++) {
      Body* first = collision_groups[i].first;
      Body* second = collision_groups[i].second;
      
      float new_mass = first->mass + second->mass;
      sf::Vector2f new_pos = ((first->position * first->mass) + (second->position * second->mass)) / new_mass;
      sf::Vector2f new_vel = ((first->velocity * first->mass) + (second->velocity * second->mass)) / new_mass;

      Body* new_body = new Body(new_pos, new_mass, new_vel);
      
      if (camera.tracked_body == first || camera.tracked_body == second) {
        camera.tracked_body = new_body;
      }

      delete first;
      delete second;

      new_bodies.push_back(new_body);
    }
    
    // 'new_bodies' will become 'bodies' for next iteration, so reassign it
    bodies = new_bodies;

    // Move each body to a new position
    // (we can't do it in prev. loop because it will break force calculation)
    for (size_t i = 0; i < bodies.size(); i++) {
      bodies[i]->move();
    }
    
    // Apply camera tracking
    camera.apply_offset(bodies, window);

    // Clear the window
    window.clear(sf::Color::Black);
    // Draw all bodies
    for (Body *body : bodies) {
      body->draw(&window);
    }
    // Display the window
    window.display();

    // Consistent framerate
    sf::Time elapsed_time = clock.restart();
    sf::Time time_to_wait = spf - elapsed_time;
    if (time_to_wait > sf::Time::Zero) {
      sf::sleep(time_to_wait);
    }
  }

  return 0;
}
