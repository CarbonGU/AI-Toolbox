#ifndef AI_TOOLBOX_MDP_SPARSE_RLMODEL_HEADER_FILE
#define AI_TOOLBOX_MDP_SPARSE_RLMODEL_HEADER_FILE

#include <tuple>
#include <random>

#include <AIToolbox/Impl/Seeder.hpp>
#include <AIToolbox/ProbabilityUtils.hpp>
#include <AIToolbox/Types.hpp>
#include <AIToolbox/MDP/Types.hpp>

namespace AIToolbox {
    namespace MDP {

#ifndef DOXYGEN_SKIP
        // This is done to avoid bringing around the enable_if everywhere.
        template <typename E, typename = typename std::enable_if<is_experience<E>::value>::type>
        class SparseRLModel;
#endif

        /**
         * @brief This class models Experience as a Markov Decision Process.
         *
         * Often an MDP is not known in advance. It is known that it can assume
         * a certain set of states, and that a certain set of actions are
         * available to the agent, but not much more. Thus, in these cases, the
         * goal is not only to find out the best policy for the MDP we have,
         * but at the same time learn the actual transition and reward
         * functions of such a model. This task is called "reinforcement
         * learning".
         *
         * This class helps with this. A naive approach to reinforcement
         * learning is to keep track, for each action, of its results, and
         * deduce transition probabilities and rewards based on the data
         * collected in such a way. This class does just this.
         *
         * This class normalizes an Experience object to produce a transition
         * function and a reward function. The transition function is
         * guaranteed to be a correct probability function, as in the sum of
         * the probabilities of all transitions from a particular state and a
         * particular action is always 1. Each instance is not directly synced
         * with the supplied Experience object. This is to avoid possible
         * overheads, as the user can optimize better depending on their use
         * case. See sync().
         *
         * A possible way to improve the data gathered using this class, is to
         * artificially modify the data as to skew it towards certain
         * distributions.  This could be done if some knowledge of the model
         * (even approximate) is known, in order to speed up the learning
         * process. Another way is to assume that all transitions are possible,
         * add data to support that claim, and simply wait until the averages
         * converge to the true values. Another thing that can be done is to
         * associate with each fake datapoint an high reward: this will skew
         * the agent into trying out new actions, thinking it will obtained the
         * high rewards. This is able to obtain automatically a good degree of
         * exploration in the early stages of an episode. Such a technique is
         * called "optimistic initialization".
         *
         * Whether any of these techniques work or not can definitely depend on
         * the model you are trying to approximate. Trying out things is good!
         *
         * The difference between this class and the MDP::RLModel class is that
         * this class stores transitions and rewards in sparse matrices. This
         * results in a possibly slower access to individual probabilities and
         * rewards, but immeasurably speeds up computation with some classes of
         * planning algorithms in case the number of useful transitions is very
         * little with respect to the total theoretic state action space of
         * SxAxS. It also of course incredibly reduces memory consumption in
         * such cases, which may also improve speed by effect of improved
         * caching.
         */
        template <typename E>
        class SparseRLModel<E> {
            public:
                using TransitionTable   = SparseMatrix3D;
                using RewardTable       = SparseMatrix3D;
                /**
                 * @brief Constructor using previous Experience.
                 *
                 * This constructor selects the Experience that will
                 * be used to learn an MDP Model from the data, and initializes
                 * internal Model data.
                 *
                 * The user can choose whether he wants to directly sync
                 * the SparseRLModel to the underlying Experience, or delay
                 * it for later.
                 *
                 * In the latter case the default transition function
                 * defines a transition of probability 1 for each
                 * state to itself, no matter the action.
                 *
                 * In general it would be better to add some amount of bias
                 * to the Experience so that when a new state-action pair is
                 * tried, the SparseRLModel doesn't automatically compute 100%
                 * probability of transitioning to the resulting state, but
                 * smooths into it. This may depend on your problem though.
                 *
                 * The default reward function is 0.
                 *
                 * @param exp The base Experience of the model.
                 * @param discount The discount used in solving methods.
                 * @param sync Whether to sync with the Experience immediately or delay it.
                 */
                SparseRLModel(const E & exp, double discount = 1.0, bool sync = false);

                /**
                 * @brief This function sets a new discount factor for the Model.
                 *
                 * @param d The new discount factor for the Model.
                 */
                void setDiscount(double d);

                /**
                 * @brief This function syncs the whole SparseRLModel to the underlying Experience.
                 *
                 * Since use cases in AI are very varied, one may not want to update
                 * its SparseRLModel for each single transition experienced by the agent. To
                 * avoid this we leave to the user the task of syncing between the
                 * underlying Experience and the SparseRLModel, as he/she sees fit.
                 *
                 * After this function is run the transition and reward functions
                 * will accurately reflect the state of the underlying Experience.
                 */
                void sync();

                /**
                 * @brief This function syncs a state action pair in the SparseRLModel to the underlying Experience.
                 *
                 * Since use cases in AI are very varied, one may not want to update
                 * its SparseRLModel for each single transition experienced by the agent. To
                 * avoid this we leave to the user the task of syncing between the
                 * underlying Experience and the SparseRLModel, as he/she sees fit.
                 *
                 * This function updates a single state action pair with the underlying
                 * Experience. This function is offered to avoid having to recompute the
                 * whole SparseRLModel if the user knows that only few transitions have been
                 * experienced by the agent.
                 *
                 * After this function is run the transition and reward functions
                 * will accurately reflect the state of the underlying Experience
                 * for the specified state action pair.
                 *
                 * @param s The state that needs to be synced.
                 * @param a The action that needs to be synced.
                 */
                void sync(size_t s, size_t a);

                /**
                 * @brief This function syncs a state action pair in the SparseRLModel to the underlying Experience in the fastest possible way.
                 *
                 * This function updates a state action pair given that the last increased transition
                 * in the underlying Experience is the triplet s, a, s1. In addition, this function only
                 * works if it needs to add information from this single new point of information (if
                 * more has changed from the last sync, use sync(s,a) ). The performance boost that
                 * this function obtains increases with the increase of the number of states in the model.
                 *
                 * @param s The state that needs to be synced.
                 * @param a The action that needs to be synced.
                 * @param s1 The final state of the transition that got updated in the Experience.
                 */
                void sync(size_t s, size_t a, size_t s1);

                /**
                 * @brief This function samples the MDP for the specified state action pair.
                 *
                 * This function samples the model for simulate experience. The transition
                 * and reward functions are used to produce, from the state action pair
                 * inserted as arguments, a possible new state with respective reward.
                 * The new state is picked from all possible states that the MDP allows
                 * transitioning to, each with probability equal to the same probability
                 * of the transition in the model. After a new state is picked, the reward
                 * is the corresponding reward contained in the reward function.
                 *
                 * @param s The state that needs to be sampled.
                 * @param a The action that needs to be sampled.
                 *
                 * @return A tuple containing a new state and a reward.
                 */
                std::tuple<size_t, double> sampleSR(size_t s, size_t a) const;

                /**
                 * @brief This function returns the number of states of the world.
                 *
                 * @return The total number of states.
                 */
                size_t getS() const;

                /**
                 * @brief This function returns the number of available actions to the agent.
                 *
                 * @return The total number of actions.
                 */
                size_t getA() const;

                /**
                 * @brief This function returns the currently set discount factor.
                 *
                 * @return The currently set discount factor.
                 */
                double getDiscount() const;

                /**
                 * @brief This function enables inspection of the underlying Experience of the SparseRLModel.
                 *
                 * @return The underlying Experience of the SparseRLModel.
                 */
                const E & getExperience() const;

                /**
                 * @brief This function returns the stored transition probability for the specified transition.
                 *
                 * @param s The initial state of the transition.
                 * @param a The action performed in the transition.
                 * @param s1 The final state of the transition.
                 *
                 * @return The probability of the specified transition.
                 */
                double getTransitionProbability(size_t s, size_t a, size_t s1) const;

                /**
                 * @brief This function returns the stored expected reward for the specified transition.
                 *
                 * @param s The initial state of the transition.
                 * @param a The action performed in the transition.
                 * @param s1 The final state of the transition.
                 *
                 * @return The expected reward of the specified transition.
                 */
                double getExpectedReward(size_t s, size_t a, size_t s1) const;

                /**
                 * @brief This function returns the transition table for inspection.
                 *
                 * @return The rewards table.
                 */
                const TransitionTable & getTransitionFunction() const;

                /**
                 * @brief This function returns the transition function for a given action.
                 *
                 * @param a The action requested.
                 *
                 * @return The transition function for the input action.
                 */
                const SparseMatrix2D & getTransitionFunction(size_t a) const;

                /**
                 * @brief This function returns the rewards table for inspection.
                 *
                 * @return The rewards table.
                 */
                const RewardTable &     getRewardFunction()     const;

                /**
                 * @brief This function returns the reward function for a given action.
                 *
                 * @param a The action requested.
                 *
                 * @return The reward function for the input action.
                 */
                const SparseMatrix2D & getRewardFunction(size_t a) const;

                /**
                 * @brief This function returns whether a given state is a terminal.
                 *
                 * @param s The state examined.
                 *
                 * @return True if the input state is a terminal, false otherwise.
                 */
                bool isTerminal(size_t s) const;

            private:
                size_t S, A;
                double discount_;

                const E & experience_;

                TransitionTable transitions_;
                RewardTable rewards_;

                mutable std::default_random_engine rand_;
        };

        template <typename E>
        SparseRLModel<E>::SparseRLModel( const E & exp, double discount, bool toSync ) : S(exp.getS()), A(exp.getA()), experience_(exp), transitions_(A, SparseMatrix2D(S, S)), rewards_(A, SparseMatrix2D(S, S)),
        rand_(Impl::Seeder::getSeed())
        {
            setDiscount(discount);

            if ( toSync ) {
                sync();
                // Sync does not touch state-action pairs which have never been
                // seen. To keep the model consistent we set all of them as
                // self-absorbing.
                for ( size_t a = 0; a < A; ++a )
                    for ( size_t s = 0; s < S; ++s )
                        if ( experience_.getVisitsSum(s, a) == 0ul )
                            transitions_[a].insert(s, s) = 1.0;
            }
            else {
                // Make transition table true probability
                for ( size_t a = 0; a < A; ++a )
                    transitions_[a].setIdentity();
            }
        }

        template <typename E>
        void SparseRLModel<E>::setDiscount(double d) {
            if ( d <= 0.0 || d > 1.0 ) throw std::invalid_argument("Discount parameter must be in (0,1]");
            discount_ = d;
        }

        template <typename E>
        void SparseRLModel<E>::sync() {
            for ( size_t a = 0; a < A; ++a )
                for ( size_t s = 0; s < S; ++s )
                    sync(s,a);
        }

        template <typename E>
        void SparseRLModel<E>::sync(size_t s, size_t a) {
            // Nothing to do
            unsigned long visitSum = experience_.getVisitsSum(s, a);
            if ( visitSum == 0ul ) return;

            // Create reciprocal for fast division
            double visitSumReciprocal = 1.0 / visitSum;

            // Normalize
            for ( size_t s1 = 0; s1 < S; ++s1 ) {
                unsigned long visits = experience_.getVisits(s, a, s1);
                // Normalize action reward over transition visits
                if ( visits != 0 ) {
                    rewards_[a].coeffRef(s, s1) = experience_.getReward(s, a, s1) / visits;
                }
                transitions_[a].coeffRef(s, s1) = static_cast<double>(visits) * visitSumReciprocal;
            }
        }

        template <typename E>
        void SparseRLModel<E>::sync(size_t s, size_t a, size_t s1) {
            unsigned long visitSum = experience_.getVisitsSum(s, a);
            // The second condition is related to numerical errors. Once in a
            // while we reset those by forcing a true update using real data.
            if ( !(visitSum % 10000ul) ) return sync(s, a);
            if ( visitSum == 1ul ) {
                transitions_[a].coeffRef(s, s) = 0.0;
                transitions_[a].coeffRef(s, s1) = 1.0;
                rewards_[a].coeffRef(s, s1) = experience_.getReward(s, a, s1);
            } else {
                double newVisits = static_cast<double>(experience_.getVisits(s, a, s1));

                // Update reward for this transition (all others stay the same).
                rewards_[a].coeffRef(s, s1) = experience_.getReward(s, a, s1) / newVisits;

                double newTransitionValue = newVisits / static_cast<double>(visitSum - 1);
                double newVectorSum = 1.0 + (newTransitionValue - transitions_[a].coeff(s, s1));
                // This works because as long as all the values in the transition have the same denominator
                // (in this case visitSum-1), then the numerators do not matter, as we can simply normalize.
                // In the end of the process the new values will be the same as if we updated directly using
                // an increased denominator, and thus we will be able to call this function again correctly.
                transitions_[a].coeffRef(s, s1) = newTransitionValue;
                transitions_[a].row(s) /= newVectorSum;
            }
        }

        template <typename E>
        std::tuple<size_t, double> SparseRLModel<E>::sampleSR(size_t s, size_t a) const {
            size_t s1 = sampleProbability(S, transitions_[a].row(s), rand_);

            return std::make_tuple(s1, rewards_[a].coeff(s, s1));
        }

        template <typename E>
        double SparseRLModel<E>::getTransitionProbability(size_t s, size_t a, size_t s1) const {
            return transitions_[a].coeff(s, s1);
        }

        template <typename E>
        double SparseRLModel<E>::getExpectedReward(size_t s, size_t a, size_t s1) const {
            return rewards_[a].coeff(s, s1);
        }

        template <typename E>
        bool SparseRLModel<E>::isTerminal(size_t s) const {
            bool answer = true;
            for ( size_t a = 0; a < A; ++a ) {
                if ( !checkEqualSmall(1.0, transitions_[a].coeff(s, s)) ) {
                    answer = false;
                    break;
                }
            }
            return answer;
        }

        template <typename E>
        size_t SparseRLModel<E>::getS() const { return S; }
        template <typename E>
        size_t SparseRLModel<E>::getA() const { return A; }
        template <typename E>
        double SparseRLModel<E>::getDiscount() const { return discount_; }
        template <typename E>
        const E & SparseRLModel<E>::getExperience() const { return experience_; }

        template <typename E>
        const typename SparseRLModel<E>::TransitionTable & SparseRLModel<E>::getTransitionFunction() const { return transitions_; }
        template <typename E>
        const typename SparseRLModel<E>::RewardTable &     SparseRLModel<E>::getRewardFunction()     const { return rewards_; }

        template <typename E>
        const SparseMatrix2D & SparseRLModel<E>::getTransitionFunction(size_t a) const { return transitions_[a]; }
        template <typename E>
        const SparseMatrix2D & SparseRLModel<E>::getRewardFunction(size_t a)     const { return rewards_[a]; }
    }
}

#endif
