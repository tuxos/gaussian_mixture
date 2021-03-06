#ifndef GMM_IMPL_HPP_
#define GMM_IMPL_HPP_

#include <math.h>

#include <gaussian_mixture/gmm.h>

namespace gmm
{
  template<int DIM>
    GMM<DIM>::GMM() :
      num_states_(0), initialized_(false)
    {
    }

  template<int DIM>
    GMM<DIM>::~GMM()
    {
    }

  template<int DIM>
    GMM<DIM> &
    GMM<DIM>::setNumStates(int num)
    {
      // allocate appropriate number of gaussians
      gaussians_.resize(num);
      // set uniform priors
      priors_.setConstant(num, 1. / num);
      // finally store number of states for convenience
      num_states_ = num;
      return *this;
    }

  template<int DIM>
    GMM<DIM> &
    GMM<DIM>::initRandom(const std::vector<typename Gaussian<DIM>::VectorType> &data)
    {
      // pick random means from the data
      for (int i = 0; i < num_states_; ++i)
        {
          int next = rand() % data.size();
          Gaussian<DIM> &g = gaussians_[i];
          // adapt mean --> covariance is left to be the identity
          g.mean() = data[next];
        }
      initialized_ = true;
      return *this;
    }

  template<int DIM>
    GMM<DIM> &
    GMM<DIM>::initKmeans(const std::vector<typename Gaussian<DIM>::VectorType> &data, int max_iter)
    {
      std::vector<int> assignments(data.size());
      std::vector<int> assignments2(data.size());
      bool changed = false;
      // set old assignments to -1 initially
      for (int i = 0; i < (int) assignments2.size(); ++i)
        {
          assignments2[i] = -1;
        }
      // first init randomly
      initRandom(data);

      // initial cluster step
      cluster(assignments, assignments2, data, changed);
      changed = true;

      // then cluster for the remaining number of iterations
      for (int iter = 1; iter < max_iter; ++iter)
        {
          // 1) --> update clusters with the new assignments
          if (iter % 2 == 0)
            {
              updateClusters(assignments2, data);
            }
          else
            {
              updateClusters(assignments, data);
            }

          // 2) --> cluster step
          // be sure to swap assignments and old_assignments every 2nd iteration
          if (iter % 2 == 0)
            {
              cluster(assignments, assignments2, data, changed);
            }
          else
            {
              cluster(assignments2, assignments, data, changed);
            }
          // check if an assignment changed --> if not we are done
          if (!changed)
            {
              DEBUG_STREAM("No assignment changed ... kmeans finished after " << iter
                  << " iterations");
              break;
            }

        }

      initialized_ = true;

      // START DEBUG ONLY
      if (DEBUG)
        {
          for (int i = 0; i < num_states_; ++i)
            {
              DEBUG_STREAM("afterKMEANS: mean of state " << i << ":");
              DEBUG_STREAM(gaussians_[i].getMean().transpose());
              DEBUG_STREAM("covariance:");
              DEBUG_STREAM(gaussians_[i].getCovariance());
            }
        }
      // END DEBUG ONLY

      return *this;
    }

  template<int DIM>
    GMM<DIM> &
    GMM<DIM>::initUniformAlongAxis(const std::vector<typename Gaussian<DIM>::VectorType> &data,
        int axis)
    {
      assert(axis >= 0 && axis < DIM);
      // first calculate mean along the selected axis
      g_float min = GFLOAT_MAX;
      g_float max = GFLOAT_MIN;
      for (int i = 0; i < (int) data.size(); ++i)
        {
          g_float tmp = data[i](axis);
          if (tmp < min)
            min = tmp;
          if (tmp > max)
            max = tmp;
        }
      // next init gaussians to those data points that are closest
      // to a uniform distribution along the axis
      for (int i = 0; i < num_states_; ++i)
        {
          // calculate desired value
          g_float desired = (max - min) * i / num_states_ + min;
          g_float best_dist = GFLOAT_MAX;
          int best = 0;
          // find data point closest to this one
          for (int j = 0; j < int(data.size()); ++j)
            {
              g_float dist = desired - data[j](axis);
              if (dist < best_dist)
                {
                  best_dist = dist;
                  best = j;
                }
            }
          // adapt mean --> covariance is left to be the identity
          gaussians_[i].setMean(data[best]);
        }
      initialized_ = true;
      return *this;
    }

  template<int DIM>
    GMM<DIM> &
    GMM<DIM>::setMean(int state, typename Gaussian<DIM>::VectorType &mean)
    {
      assert(state >= 0 && state < num_states_);
      gaussians_[state].setMean(mean);
      return *this;

    }

  template<int DIM>
    GMM<DIM> &
    GMM<DIM>::setCovariance(int state, typename Gaussian<DIM>::MatrixType &cov)
    {
      assert(state >= 0 && state < num_states_);
      gaussians_[state].setCovariance(cov);
      return *this;
    }

  template<int DIM>
    GMM<DIM> &
    GMM<DIM>::setPrior(int state, g_float prior)
    {
      assert(state >= 0 && state < num_states_);
      priors_[state] = prior;
      return *this;
    }

  template<int DIM>
    GMM<DIM> &
    GMM<DIM>::setPriors(Eigen::VectorXd prior)
    {
      assert(prior.size() == num_states_);
      priors_ = prior;
      return *this;
    }

  template<int DIM>
    g_float
    GMM<DIM>::cluster(std::vector<int> &assignments, std::vector<int> &old_assignments,
        const std::vector<typename Gaussian<DIM>::VectorType>& pats, bool &changed)
    {
      int best_idx = 0;
      g_float summed_dist = 0.;

      // initially assume no assignment changed
      changed = false;

      for (int i = 0; i < (int) pats.size(); ++i)
        { // for each pattern find the best assignment to a prototype
          g_float dist = GFLOAT_MAX;
          // search through all cluster centers
          for (int g_idx = 0; g_idx < (int) gaussians_.size(); ++g_idx)
            {
              g_float tmp_dist = (pats[i] - gaussians_[g_idx].mean()).squaredNorm();
              if (tmp_dist < dist)
                {
                  dist = tmp_dist;
                  best_idx = g_idx;
                }
            }
          assignments[i] = best_idx;
          summed_dist += dist;
        }

      for (int i = 0; i < (int) pats.size(); ++i)
        {
          if (assignments[i] != old_assignments[i])
            {
              changed = true;
              break;
            }
        }
      //DEBUG_STREAM("DONE COMPUTING CLUSTERS");
      return summed_dist;
    }

  template<int DIM>
    void
    GMM<DIM>::updateClusters(std::vector<int> & assignments, const std::vector<typename Gaussian<
        DIM>::VectorType>& pats)
    {
      std::vector<int> patterns_per_cluster(num_states_);
      std::vector<typename Gaussian<DIM>::MatrixType> tmp_covar(num_states_);
      typename Gaussian<DIM>::VectorType tmp;
      int curr_assignment = 0;

      for (int i = 0; i < num_states_; i++)
        { // reset pattern per cluster counts
          patterns_per_cluster[i] = 0;
          gaussians_[i].mean().setZero();
          tmp_covar[i].setZero();
        }

      // update the mean
      for (int i = 0; i < (int) pats.size(); i++)
        { // traverse all training patterns
          curr_assignment = assignments[i];
          // update number of patterns per cluster
          ++patterns_per_cluster[curr_assignment];
          // update center of closest prototype
          gaussians_[curr_assignment].mean() += pats[i];
        }

      for (int i = 0; i < num_states_; i++)
        { // normalize mean
          if (patterns_per_cluster[i] > 0.)
            { // beware of the evil division by zero :)
              gaussians_[i].mean() /= patterns_per_cluster[i];
            }
        }

      // update the covariance matrix
      for (int i = 0; i < (int) pats.size(); i++)
        { // traverse all training patterns
          curr_assignment = assignments[i];
          // update covariance of closest prototype
          tmp = pats[i] - gaussians_[curr_assignment].mean();
          tmp_covar[curr_assignment] += tmp * tmp.transpose();
        }

      for (int i = 0; i < num_states_; i++)
        { // normalize covariance
          if (patterns_per_cluster[i] > 0.)
            { // beware of the evil division by zero :)
              tmp_covar[i] /= patterns_per_cluster[i];
            }
          else
            {
              tmp_covar[i] = Gaussian<DIM>::MatrixType::Identity();
            }
          gaussians_[i].setCovariance(tmp_covar[i]);
        }
      //ROS_DEBUG("DONE updating kmeans");
    }

  template<int DIM>
    void
    GMM<DIM>::draw(typename Gaussian<DIM>::VectorType &result) const
    {
      if (!initialized_)
        return;
      int state = 0;
      g_float accum = 0.;
      g_float thresh = random_uniform_0_1();
      // first accumulate priors until thresh is reached
      while ((accum < thresh) && (state < num_states_))
        {
          accum += priors_[state];
          ++state;
        }
      --state;
      // finally draw from the distribution belonging to the selected state
      return gaussians_[state].draw(result);
    }

  template<int DIM>
    g_float
    GMM<DIM>::pdf(const typename Gaussian<DIM>::VectorType x) const
    {
      if (!initialized_)
        return 0.;
      g_float likeliehood = 0., tmp = 0.;
      for (int i = 0; i < num_states_; ++i)
        {
          // multiply individual pdf with prior
          tmp = priors_[i] * gaussians_[i].pdf(x);
          // add to result
          likeliehood += tmp;
        }
      return likeliehood;
    }

  template<int DIM>
    int
    GMM<DIM>::mostLikelyGauss(const typename Gaussian<DIM>::VectorType x) const
    {
      if (!initialized_)
        return 0;
      g_float best_likeliehood = 0., tmp = 0.;
      int best = 0;
      for (int i = 0; i < num_states_; ++i)
        {
          // multiply individual pdf with prior
          tmp = priors_[i] * gaussians_[i].pdf(x);
          if (tmp > best_likeliehood)
            {
              best_likeliehood = tmp;
              best = i;
            }
        }
      return best;
    }

  template<int DIM>
    template<int P_DIM>
      GMR<DIM, P_DIM>
      GMM<DIM>::getRegressionModel() const
      {
        return GMR<DIM, P_DIM> ().setInputGMM(*this);
      }

  template<int DIM>
    EM<DIM>
    GMM<DIM>::getEM()
    {
      return EM<DIM> ().setInputGMM(*this);
    }

  template<int DIM>
    const Gaussian<DIM> &
    GMM<DIM>::getGaussian(int state) const
    {
      assert(state >= 0 && state < num_states_);
      return gaussians_[state];
    }

  template<int DIM>
    Gaussian<DIM> &
    GMM<DIM>::gaussian(int state)
    {
      assert(state >= 0 && state < num_states_);
      return gaussians_[state];
    }

  template<int DIM>
    typename Gaussian<DIM>::VectorType &
    GMM<DIM>::getMean(int state)
    {
      assert(state >= 0 && state < num_states_);
      return gaussians_[state].mean();
    }

  template<int DIM>
    typename Gaussian<DIM>::MatrixType &
    GMM<DIM>::getCovariance(int state)
    {
      assert(state >= 0 && state < num_states_);
      return gaussians_[state].getCovariance();
    }

  template<int DIM>
    int
    GMM<DIM>::getNumStates() const
    {
      return num_states_;
    }

  template<int DIM>
    g_float
    GMM<DIM>::getPrior(int state) const
    {
      assert(initialized_ && state >= 0 && state < num_states_);
      return priors_(state);
    }

  template<int DIM>
    const Eigen::VectorXd
    GMM<DIM>::getPriors() const
    {
      assert(initialized_);
      return priors_;
    }

  template<int DIM>
    void
    GMM<DIM>::forceInitialize()
    {
      initialized_ = true;
    }

  template<int DIM>
    bool
    GMM<DIM>::toBinaryFile(const std::string &fname)
    {
      bool res = true;
      std::ofstream out(fname.c_str(), std::ios_base::binary);
      res &= toStream(out);
      out.close();
      return res;
    }

  template<int DIM>
    bool
    GMM<DIM>::toStream(std::ofstream &out)
    {
      int dim = DIM;
      bool res = true;
      out.write((char*) (&dim), sizeof(int));
      out.write((char*) (&num_states_), sizeof(int));
      out.write((char*) (&initialized_), sizeof(bool));
      for (int i = 0; i < priors_.size(); ++i)
        {
          out.write((char*) (&priors_[i]), sizeof(double));
        }
      for (int i = 0; i < num_states_; ++i)
        {
          res &= gaussians_[i].toStream(out);
        }
      return res;
    }

  template<int DIM>
    bool
    GMM<DIM>::fromBinaryFile(const std::string &fname)
    {
      std::ifstream in;
      bool res = true;
      in.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);

      try
        {
          in.open(fname.c_str(), std::ios_base::binary);
          res &= fromStream(in);
        }
      catch (std::ifstream::failure& e)
        {
          ERROR_STREAM("Failed to load Gaussian Mixture Model from file " << fname);
          in.close();
          return false;
        }
      in.close();
      return res;
    }

  template<int DIM>
    bool
    GMM<DIM>::fromStream(std::ifstream &in)
    {
      int dim;
      bool res = true;
      in.read((char*) (&dim), sizeof(int));
      if (dim != DIM)
        {
          ERROR_STREAM("called GMM.fromBinaryFile() with message of invalid dimension: " << dim
              << " this dim: " << DIM);
          return false;
        }
      in.read((char*) (&num_states_), sizeof(int));
      // set number of states such that gaussians_ and priors_ is allocated correctly
      setNumStates(num_states_);
      in.read((char*) (&initialized_), sizeof(bool));
      for (int i = 0; i < priors_.size(); ++i)
        {
          in.read((char*) (&priors_[i]), sizeof(double));
        }
      for (int i = 0; i < num_states_; ++i)
        {
          res &= gaussians_[i].fromStream(in);
        }
      return res;
    }

#ifdef GMM_ROS
  template<int DIM>
    bool
    GMM<DIM>::fromMessage(const gaussian_mixture::GaussianMixtureModel &msg)
    {
      if (msg.dim != DIM)
        {
          ERROR_STREAM("cannot initialize gmm of dim " << DIM << " from model with dim " << msg.dim);
          return false;
        }
      if (msg.num_states < 1)
        {
          ERROR_STREAM("cannot read model with 0 states from message!");
          return false;
        }
      bool res = true;
      setNumStates(msg.num_states);
      initialized_ = msg.initialized;

      // copy prior probabilities
      for (int i = 0; i < num_states_; ++i)
        {
          priors_(i) = msg.priors[i];
        }

      // read all gaussians from message
      for (int i = 0; i < num_states_; ++i)
        {
          res &= gaussians_[i].fromMessage(msg.gaussians[i]);
          if (!res) // fail early
            return false;
        }
      return res;
    }

  template<int DIM>
    bool
    GMM<DIM>::toMessage(gaussian_mixture::GaussianMixtureModel &msg) const
    {
      if (num_states_ < 1)
        {
          ERROR_STREAM("cannot write model with 0 states to message!");
          return false;
        }
      bool res = true;
      msg.dim = DIM;
      msg.num_states = num_states_;
      msg.initialized = initialized_;

      // copy prior probabilities
      msg.priors.resize(num_states_);
      for (int i = 0; i < num_states_; ++i)
        {
          msg.priors[i] = priors_(i);
        }

      msg.gaussians.resize(num_states_);
      // write all gaussians to a message
      for (int i = 0; i < num_states_; ++i)
        {
          res &= gaussians_[i].toMessage(msg.gaussians[i]);
          if (!res) // fail early
            return false;
        }
      return res;
    }

  template<int DIM>
    bool
    GMM<DIM>::toBag(const std::string &bag_file)
    {
      ros::Time::init();
      try
        {
          rosbag::Bag bag(bag_file, rosbag::bagmode::Write);
          gaussian_mixture::GaussianMixtureModel msg;
          if (!toMessage(msg))
            {
              ERROR_STREAM("Could not convert GMM to message.");
              return false;
            }
          bag.write("gaussian_mixture_model", ros::Time::now(), msg);
          bag.close();
        }
      catch (rosbag::BagIOException e)
        {
          ROS_ERROR("Could not open bag file %s: %s", bag_file.c_str(), e.what());
          return false;
        }
      return true;
    }

  template<int DIM>
    bool
    GMM<DIM>::fromBag(const std::string &bag_file)
    {
      try
        {
          rosbag::Bag bag(bag_file, rosbag::bagmode::Read);
          rosbag::View view(bag, rosbag::TopicQuery("gaussian_mixture_model"));
          int count = 0;
          BOOST_FOREACH(rosbag::MessageInstance const msg, view)
          {
            if (count > 1)
            {
              ERROR_STREAM("More than one GMM stored in bag file!");
              return false;
            }
          ++count;

          gaussian_mixture::GaussianMixtureModelConstPtr model = msg.instantiate<
          gaussian_mixture::GaussianMixtureModel> ();
          if (!fromMessage(*model))
            {
              ERROR_STREAM("Could not initialize GMM from message!");
              return false;
            }
        }
      bag.close();
    }
  catch (rosbag::BagIOException e)
    {
      ROS_ERROR("Could not open bag file %s: %s", bag_file.c_str(), e.what());
      return false;
    }
  return true;
}

#endif

}

#endif /* GMM_IMPL_HPP_ */
