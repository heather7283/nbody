#include <SFML/Graphics.hpp>
#include <SFML/System/Vector2.hpp>
#include <iostream>
#include <random>


/* TODO
* Sometimes one body can end up in several collision groups simultaneously,
* causing it to be deleted twice and crashing program with double free()
*
* It is unlikely to happen with low body count, 
* but proabbility approaches 100% the more bodies you add.
*
* To prevent this the whole collision logic needs to be reworked
*/


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
  bool is_tracked = false;
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
    this->velocity -= direction * force;
    other->velocity += direction * force;
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


int main() {
  // Create a window
  sf::RenderWindow window(sf::VideoMode(800, 600), "nbody");
  sf::Clock clock;
  
  // Calculate fps
  const float fps = 60.0f;
  sf::Time spf = sf::seconds(1.0f / fps);

  // Array of bodies
  std::vector<Body*> bodies;
  
  generate_random_bodies(10000, &bodies);
  //bodies = {
  //  new Body(sf::Vector2f(100, 100), 64),
  //  new Body(sf::Vector2f(200, 100), 64),
  //  new Body(sf::Vector2f(100, 300), 64),
  //  new Body(sf::Vector2f(200, 300), 64),
  //};
  
  Camera camera(bodies[0]);

  std::vector<Body*> new_bodies;
  std::vector<std::vector<Body*>> collision_groups;
  
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

        // This collision check algorithm is terrible and slow
        // but I can't think of a better way of handling multi-body collisions
        // collisions don't happen often anyway so it should be *good enough*
        
        // If first and second collided:
        if (first->check_for_collision(second)) {
          // Check if collision group with either of bodies already exists
          // Iterate over all collision groups
          bool grp_exists = false;
          for (size_t grp_i = 0; grp_i < collision_groups.size(); grp_i++) {
            // Iterate over all bodies in this collision group
            bool first_in_grp = false, second_in_grp = false;
            for (size_t elem_i = 0; elem_i < collision_groups[grp_i].size(); elem_i++) {
              first_in_grp = (collision_groups[grp_i][elem_i]->id == first->id) || first_in_grp;
              second_in_grp = (collision_groups[grp_i][elem_i]->id == second->id) || second_in_grp;
            }

            // WORKAROUND
            if ((first->is_collided) || (second->is_collided)) {
              grp_exists = true;
              break;
            }

            // If one of bodies is in a group, also add missing body there
            // and exit the for loop early
            if (first_in_grp && !second_in_grp) {
              collision_groups[grp_i].push_back(second);
              grp_exists = true;
              break;
            }
            else if (!first_in_grp && second_in_grp) {
              collision_groups[grp_i].push_back(first);
              grp_exists = true;
              break;
            }
          }
          // If collision group with either of bodies doesn't exist,
          // create it and add 2 colliding bodies there
          if (!grp_exists) {
            collision_groups.push_back(std::vector<Body*>{first, second});
          }
          
          // Set collided status on both bodies
          first->is_collided = true;
          second->is_collided = true;
        }
        
        // Apply gravitational forces to both bodies
        first->do_gravity(second);
      }
      
      // When we are done processing all 'second' combinations for 'first',
      // check if 'first' collided with anything. If it didn't, add it to new_bodies
      if (!first->is_collided) {
        new_bodies.push_back(first);
      }
    }
    // Handle last body (see NOTE in the loop above)
    if (!bodies[bodies.size() - 1]->is_collided) {
      new_bodies.push_back(bodies[bodies.size() - 1]);
    }
    
    // Process all collision groups
    // Collision group is a list of collided objects, 
    // they need to be merged into one
    for (size_t i = 0; i < collision_groups.size(); i++) {
      unsigned int grp_size = collision_groups[i].size();
      if (grp_size > 0) {
        bool switch_camera = false;
        float mass_sum = 0;
        sf::Vector2f velocity_sum(0, 0);
        sf::Vector2f position_sum(0, 0);

        // Iterate over bodies in a group and accumulate their characteristics in varaibles
        for (size_t j = 0; j < collision_groups[i].size(); j++) {
          Body* body = collision_groups[i][j];
          mass_sum += body->mass;
          velocity_sum += body->velocity;
          position_sum += body->position;
          switch_camera = (camera.tracked_body->id == body->id || switch_camera);
          // Get rid of this body; we won't need it anymore
          delete body;
        }
        
        // Construct a new body based on accumulated values
        Body* new_body = new Body(position_sum / (float)grp_size,
                                  mass_sum / grp_size,
                                  velocity_sum / mass_sum);
        // If any of bodies in group had camera focus, switch focus to newly created body
        if (switch_camera) {
          camera.tracked_body = new_body;
        }
        // Finally, add our new body to new_bodies
        new_bodies.push_back(new_body);
        
        std::cout << "Collision formed new body with id " << new_body->id << "\n";
      }
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
