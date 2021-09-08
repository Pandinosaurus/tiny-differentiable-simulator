#ifndef REACHER_ENVIRONMENT_H
#define REACHER_ENVIRONMENT_H

#include <iostream>
#include "math/neural_network.hpp"
#include "dynamics/forward_dynamics.hpp"
#include "dynamics/integrator.hpp"
#include "urdf/urdf_cache.hpp"
#include "utils/file_utils.hpp"
#include "urdf/urdf_parser.hpp"
#include "math.h"

//#define COMPATIBILITY
template <typename Algebra>
struct ReacherContactSimulation {
  using Scalar = typename Algebra::Scalar;
  using Vector3 = typename Algebra::Vector3;
  using Transform = typename Algebra::Transform;

  tds::UrdfCache<Algebra> cache;
  std::string m_urdf_filename;
  tds::World<Algebra> world;
  tds::MultiBody<Algebra>* mb_ = nullptr;
  Vector3 endeffector_pos;
  double torqueSqSum;

  int num_timesteps{1};
  Scalar dt{Algebra::from_double(1. / 100.)};

  int input_dim() const { return mb_->dof() + mb_->dof_qd(); }
  int state_dim() const {
    return mb_->dof() + mb_->dof_qd() + mb_->num_links() * 7;
  }
  int output_dim() const { return num_timesteps * state_dim(); }

  ReacherContactSimulation() {
    std::string plane_filename;
    world.set_gravity(Vector3(0., 0., -10));

    //tds::StdLogger logger;
    tds::NullLogger logger;
    {
      std::string urdf_name = "gym/reacher.urdf";
      tds::FileUtils::find_file(urdf_name, m_urdf_filename);
      std::cout << "m_urdf_filename = " << m_urdf_filename << std::endl;
      bool is_floating = false;
      mb_ = cache.construct(m_urdf_filename, world, false, is_floating);
      mb_->set_floating_base(is_floating);
      mb_->initialize();
    }

    // mb_->base_X_world().translation = Algebra::unit3_z();
    std::cout << "ReacherContactSimulation!" << std::endl;
    std::cout << "\tinput_dim = "<< this->input_dim() << std::endl;
    std::cout << "\tstate_dim = " << this->state_dim() << std::endl;
    std::cout << "\tnum_dofs" << mb_->dof() << std::endl;
    for (int i = 0; i < mb_->dof(); i++) {
      std::cout << "dof[" << i << "] = " << mb_->q(i) << ", " << mb_->qd(i) << std::endl;
    }
  }

  virtual ~ReacherContactSimulation() {
    //std::cout << "~ReacherContactSimulation" << std::endl;
  }
  std::vector<Scalar> operator()(const std::vector<Scalar>& v,
                                 const std::vector<Scalar>& pd_target_angles) {
    assert(static_cast<int>(v.size()) == input_dim());
    mb_->initialize();
    // copy input into q, qd
    for (int i = 0; i < mb_->dof(); ++i) {
      mb_->q(i) = v[i];
    }
    for (int i = 0; i < mb_->dof_qd(); ++i) {
      mb_->qd(i) = v[i + mb_->dof()];
    }

    static double t = 0;
    // printf("t=%f, [%f,%f]\n",t,mb_->q(0),mb_->q(1));
    t += dt;
    std::vector<Scalar> result(output_dim());
    for (int t = 0; t < num_timesteps; ++t) {
      double kp = 0.01;
      double kd = 0.1 * kp;
      mb_->tau_[0] = kp * (pd_target_angles[0] - mb_->q(0)) - kd * mb_->qd(0);
      mb_->tau_[1] = kp * (pd_target_angles[1] - mb_->q(1)) - kd * mb_->qd(1);
      // mb_->tau_[0] = 0.001;
      // mb_->tau_[1] = -0.001;

      torqueSqSum = mb_->tau_.sqnorm();
      // std::vector<Scalar> tau = policy(observation)

      tds::forward_dynamics(*mb_, world.get_gravity());
      mb_->clear_forces();

      //integrate_euler_qdd(*mb_, dt);

      //world.step(dt);

      tds::integrate_euler(*mb_, dt);

      // copy q, qd, link world poses (for rendering) to output
      int j = 0;
      for (int i = 0; i < mb_->dof(); ++i, ++j) {
        result[j] = mb_->q(i);
      }
      for (int i = 0; i < mb_->dof_qd(); ++i, ++j) {
        result[j] = mb_->qd(i);
      }
#if 1
      for (const auto& link : *mb_) {
        if (link.X_visuals.size()) {
          Transform visual_X_world = link.X_world * link.X_visuals[0];
          result[j++] = visual_X_world.translation[0];
          result[j++] = visual_X_world.translation[1];
          result[j++] = visual_X_world.translation[2];
          // std::cout << "trans = "
          //           << visual_X_world.translation[0] << ", "
          //           << visual_X_world.translation[1] << ", "
          //           << visual_X_world.translation[2] << std::endl;
          auto orn = Algebra::matrix_to_quat(visual_X_world.rotation);
          result[j++] = orn[0];
          result[j++] = orn[1];
          result[j++] = orn[2];
          result[j++] = orn[3];
        } else {
          // check if we have links without visuals
          assert(0);
          j += 7;
        }
      }

      endeffector_pos[0] = result[j - 7];
      endeffector_pos[1] = result[j - 6];
      endeffector_pos[2] = result[j - 5];

      computeEndEffectorPos();

     

#endif
    }
    return result;
  }

  void computeEndEffectorPos()
  {
    tds::forward_kinematics(*mb_);
    const tds::Link<Algebra>& link = mb_->links()[mb_->num_links()-1];
    
    // Assumption: the end-effector is the last link
    endeffector_pos[0] = link.X_world.translation.x();
    endeffector_pos[1] = link.X_world.translation.y();
    endeffector_pos[2] = link.X_world.translation.z();
    // std::cout << "ee = "
    //           << endeffector_pos[0] << ", "
    //           << endeffector_pos[1] << ", "
    //           << endeffector_pos[2] << std::endl;
  }

};

struct ReacherEnvOutput
{
    std::vector<double> obs;
    double reward;
    bool done;
};

struct ReacherRolloutOutput
{
    ReacherRolloutOutput()
        :total_reward(0),
        num_steps(0)
    {
    }
    double total_reward;
    int num_steps;
};


template <typename Algebra>
struct ReacherEnv {
  using Scalar = typename Algebra::Scalar;
  using Vector3 = typename Algebra::Vector3;

  ReacherContactSimulation<Algebra> contact_sim;
  Scalar action_low_;
  Scalar action_high_;

  int action_dim_{2};
  int observation_dim_{10};
  int counter_{-1};
  Vector3 endeffector_target_;

  ReacherEnv() {
    bool use_input_bias = false;
    static int counter=0;
    counter_ = counter++;
    printf("ReacHerEnv counter_=%d\n", counter_);
    //observation_dim_ = contact_sim.input_dim();
    neural_network.set_input_dim(observation_dim_, use_input_bias);
    bool learn_bias = true;
    neural_network.add_linear_layer(tds::NN_ACT_IDENTITY, action_dim_,
                                    learn_bias);  // action is 1 number

    action_high_ = 10;
    action_low_ = -action_high_;
  }
  virtual ~ReacherEnv() {
      printf("~ReacHerEnv counter_=%d\n", counter_);
  }

  std::vector<Scalar> sim_state;
  std::vector<Scalar> sim_state_with_graphics;

  std::vector<double> reset() {
    //std::srand(0);
    int input_dim = contact_sim.input_dim();
    sim_state.resize(input_dim);
    for (int i = 0; i < sim_state.size(); i++) {
      sim_state[i] = 0.05 * ((std::rand() * 1. / RAND_MAX) - 0.5) * 2.0;
    }

    // Set endeffector_target_ [(-0.2, 0.2), (-0.2, 0.2), 0.01]
    endeffector_target_[0] = 0.2 * ((std::rand() * 1. / RAND_MAX) - 0.5) * 2.0;
    endeffector_target_[1] = 0.2 * ((std::rand() * 1. / RAND_MAX) - 0.5) * 2.0;
    endeffector_target_[2] = 0.01;
    //std::cout << "reset: endeffector target = " << 
    //    endeffector_target_[0] << " " << 
    //    endeffector_target_[1] << " " << 
    //    endeffector_target_[2] << std::endl;
     //for (auto v : sim_state)
     //    std::cout << v << ",";
    //std::cout << std::endl;
    // Fill observations
    std::vector<double> obs_(observation_dim_);
    fill_obs(obs_);

    return obs_;
  }

  std::vector<double> reset2() {
    
    std::vector<double> obs = reset();
    std::vector<double> sim_state;
    // change layout to [q1,qd1, q0, qd0] to be compatible with
#ifdef COMPATIBILITY
#error
    sim_state.push_back(obs[1]);
    sim_state.push_back(obs[3]);
    sim_state.push_back(obs[0]);
    sim_state.push_back(obs[2]);
#else
    sim_state.push_back(obs[0]);
    sim_state.push_back(obs[1]);
    sim_state.push_back(obs[2]);
    sim_state.push_back(obs[3]);
#endif
    
    return sim_state;
  }

  void seed(long long int s) {
      //std::cout<<"seed:" << s << std::endl;
      std::srand(s);
  }

  ReacherEnvOutput step2(double action) {
      //std::cout << "action:" << action << std::endl;

      ReacherEnvOutput env_out;
      
      std::vector<double> obs;
      step(action, obs, env_out.reward, env_out.done);
      // obs in format [q0,q1,qd0,qd1]
      // change layout to [q1,qd1, q0, qd0] to be compatible with
      // PyBullet'ReacHerContinuousBulletEnv-v0'
#ifdef COMPATIBILITY
#error
      env_out.obs.push_back(obs[1]);
      env_out.obs.push_back(obs[3]);
      env_out.obs.push_back(obs[0]);
      env_out.obs.push_back(obs[2]);
#else
      env_out.obs.push_back(obs[0]);
      env_out.obs.push_back(obs[1]);
      env_out.obs.push_back(obs[2]);
      env_out.obs.push_back(obs[3]);
#endif
      //std::cout << "env_out.done=" << env_out.done << std::endl;
      //std::cout << "env_out.reward=" << env_out.done << std::endl;
      //std::cout << "obs=" << obs << std::endl;
      return env_out;
  }
  
  ReacherRolloutOutput rollout(int rollout_length, double shift) {
      //std::cout << "action:" << action << std::endl;

      ReacherRolloutOutput rollout_out;
      std::vector<double> obs = reset();
      bool done = false;
      int steps=0;
      while (rollout_out.num_steps<rollout_length && !done)
      {
         //double action = 0.f;
         auto action = policy(obs);
         double reward;
         step(action, obs, reward, done);
         rollout_out.total_reward += reward;
         rollout_out.num_steps++;
      }
      return rollout_out;
  }
  
  void fill_obs(std::vector<double>& obs) {
    obs.resize(observation_dim_);
    auto dof = contact_sim.mb_->dof();
    int j = 0;
    for (int i = 0; i < dof; i++) 
        obs[j++] = cos(contact_sim.mb_->q_[i]);
    for (int i = 0; i < dof; i++) 
        obs[j++] = sin(contact_sim.mb_->q_[i]);
    for (int i = 0; i < dof; i++) 
        obs[j++] = contact_sim.mb_->q_[i];
    for (int i = 0; i < dof; i++) 
        obs[j++] = contact_sim.mb_->qd_[i];
    
    contact_sim.computeEndEffectorPos();

    auto diff = contact_sim.endeffector_pos - endeffector_target_;
    obs[j++] = diff[0];
    obs[j++] = diff[1];
  }

  void step(std::vector<double>& action, std::vector<double>& obs, double& reward,
            bool& done) {
    // //clip
    // if (action < action_low_)
    //     action = action_low_;
    // if (action > action_high_)
    //     action = action_high_;

    // sim_state = [q0, q1, qd0, qd1]

    //printf("action=%f,%f\n", action[0],action[1]);
    sim_state_with_graphics = contact_sim(sim_state, action);
    sim_state = sim_state_with_graphics;

    sim_state.resize(contact_sim.input_dim());
    
    //printf("sim_state=\n", action[0],action[1]);
    //for (int i=0;i<sim_state.size();i++)
    //   printf("%f,",sim_state[i]);
    //printf("\n");
    // Observation
    std::vector<double> obs_(observation_dim_);
    fill_obs(obs_);
    obs = obs_;

    // Reward
    auto dist = (contact_sim.endeffector_pos - endeffector_target_).length();
    auto reward_dist = -dist;
    //auto reward_ctrl = -contact_sim.torqueSqSum;
    reward = reward_dist;// + reward_ctrl;
    // std::cout << "\treward = " << reward << std::endl;

    // Terminal
    done = false; // No terminal condition for reacher
  }

  tds::NeuralNetwork<Algebra> neural_network;

  void init_neural_network_rand() {
    int num_params = neural_network.num_weights() + neural_network.num_biases();
    std::vector<double> x(num_params);
    //rand
    for (int i=0;i<x.size();i++)
    {
        x[i] = ((std::rand() * 1. / RAND_MAX)-0.5)*2.0;
    }
    init_neural_network(x);
  }

  void init_neural_network(const std::vector<double>& x) {
    neural_network.set_parameters(x);
  }

  inline const std::vector<double> policy(const std::vector<double>& obs)
  {
      std::vector<double> action (action_dim_, Scalar(0));

      neural_network.compute(obs, action);

      return action;
  }
  // inline double policy(const std::vector<double>& obs) {
  //   std::vector<double> action;
  //   neural_network.compute(obs, action);

  //   //normalize actions
    
  //   for (int i=0;i<action.size();i++)
  //   {

  //       if (action[i]<-1.0)
  //           action[i]=-1.0;
  //       if (action[i]>1.0)
  //           action[i]=1.0;
  //       action[i] *= (action_high_ - action_low_)/2.0;
  //       action[i] += (action_low_ + action_high_)/2.0;

  //   }

  //   return action[0];
  // }

  inline double policy2(const std::vector<double>& x,
                        const std::vector<double>& obs) {
    double action = 0;

    for (int i = 0; i < 4; i++) {
      action += x[i] * obs[i];  // identity activation
    }
    action += x[4];  // bias

    if (action<-1.0)
        action=-1.0;
    if (action>1.0)
        action=1.0;
    action *= (action_high_ - action_low_)/2.0;
    action += (action_low_ + action_high_)/2.0;

    return action;
  }
};

#endif  // REACHER_ENVIRONMENT_H