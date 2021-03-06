#pragma once

#include <cilantro/icp_base.hpp>
#include <cilantro/rigid_registration_utilities.hpp>

namespace cilantro {
    template <typename ScalarT, class CorrespondenceSearchEngineT>
    class CombinedMetricRigidICP3 : public IterativeClosestPointBase<CombinedMetricRigidICP3<ScalarT,CorrespondenceSearchEngineT>,RigidTransformation<ScalarT,3>,CorrespondenceSearchEngineT,VectorSet<ScalarT,1>> {
        friend class IterativeClosestPointBase<CombinedMetricRigidICP3<ScalarT,CorrespondenceSearchEngineT>,RigidTransformation<ScalarT,3>,CorrespondenceSearchEngineT,VectorSet<ScalarT,1>>;
    public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW

        CombinedMetricRigidICP3(const ConstVectorSetMatrixMap<ScalarT,3> &dst_p,
                                const ConstVectorSetMatrixMap<ScalarT,3> &dst_n,
                                const ConstVectorSetMatrixMap<ScalarT,3> &src_p,
                                CorrespondenceSearchEngineT &corr_engine)
                : IterativeClosestPointBase<CombinedMetricRigidICP3<ScalarT,CorrespondenceSearchEngineT>,RigidTransformation<ScalarT,3>,CorrespondenceSearchEngineT,VectorSet<ScalarT,1>>(corr_engine),
                  dst_points_(dst_p), dst_normals_(dst_n), src_points_(src_p),
                  src_points_trans_(src_points_.rows(), src_points_.cols()),
                  max_optimization_iterations_(1), optimization_convergence_tol_((ScalarT)1e-5),
                  point_to_point_weight_((ScalarT)0.1), point_to_plane_weight_((ScalarT)1.0)
        {
            this->transform_init_.setIdentity();
            this->correspondences_.reserve(std::max(dst_points_.cols(), src_points_.cols()));
        }

        inline ScalarT getPointToPointMetricWeight() const { return point_to_point_weight_; }

        inline CombinedMetricRigidICP3& setPointToPointMetricWeight(ScalarT weight) {
            point_to_point_weight_ = weight;
            return *this;
        }

        inline ScalarT getPointToPlaneMetricWeight() const { return point_to_plane_weight_; }

        inline CombinedMetricRigidICP3& setPointToPlaneMetricWeight(ScalarT weight) {
            point_to_plane_weight_ = weight;
            return *this;
        }

        inline size_t getMaxNumberOfOptimizationStepIterations() const { return max_optimization_iterations_; }

        inline CombinedMetricRigidICP3& setMaxNumberOfOptimizationStepIterations(size_t max_iter) {
            max_optimization_iterations_ = max_iter;
            return *this;
        }

        inline ScalarT getOptimizationStepConvergenceTolerance() const { return optimization_convergence_tol_; }

        inline CombinedMetricRigidICP3& setOptimizationStepConvergenceTolerance(ScalarT conv_tol) {
            optimization_convergence_tol_ = conv_tol;
            return *this;
        }

    private:
        ConstVectorSetMatrixMap<ScalarT,3> dst_points_;
        ConstVectorSetMatrixMap<ScalarT,3> dst_normals_;
        ConstVectorSetMatrixMap<ScalarT,3> src_points_;
        VectorSet<ScalarT,3> src_points_trans_;

        size_t max_optimization_iterations_;
        ScalarT optimization_convergence_tol_;
        ScalarT point_to_point_weight_;
        ScalarT point_to_plane_weight_;

        // ICP interface
        void updateEstimate() {
#pragma omp parallel for
            for (size_t i = 0; i < src_points_.cols(); i++) {
                src_points_trans_.col(i) = this->transform_*src_points_.col(i);
            }

            RigidTransformation<ScalarT,3> tform_iter;
            estimateRigidTransformCombinedMetric3D<ScalarT,typename CorrespondenceSearchEngineT::CorrespondenceScalar>(dst_points_, dst_normals_, src_points_trans_, this->correspondences_, point_to_point_weight_, point_to_plane_weight_, tform_iter, max_optimization_iterations_, optimization_convergence_tol_);

            this->transform_ = tform_iter*this->transform_;
            this->transform_.linear() = this->transform_.rotation();
            this->last_delta_norm_ = std::sqrt((tform_iter.linear() - Eigen::Matrix<ScalarT,3,3>::Identity()).squaredNorm() + tform_iter.translation().squaredNorm());
        }

        // ICP interface
        VectorSet<ScalarT,1> computeResiduals() {
            if (dst_points_.cols() == 0) {
                return VectorSet<ScalarT,1>::Constant(1, src_points_.cols(), std::numeric_limits<ScalarT>::quiet_NaN());
            }
            VectorSet<ScalarT,1> res(1, src_points_.cols());
            KDTree<ScalarT,3,KDTreeDistanceAdaptors::L2> dst_tree(dst_points_);
            NearestNeighborSearchResult<ScalarT> nn;
            Vector<ScalarT,3> src_p_trans;
#pragma omp parallel for shared (res) private (nn, src_p_trans)
            for (size_t i = 0; i < src_points_.cols(); i++) {
                src_p_trans = this->transform_*src_points_.col(i);
                dst_tree.nearestNeighborSearch(src_p_trans, nn);
                ScalarT point_to_plane_dist = dst_normals_.col(nn.index).dot(dst_points_.col(nn.index) - src_p_trans);
                res[i] = point_to_point_weight_*(dst_points_.col(nn.index) - src_p_trans).squaredNorm() + point_to_plane_weight_*point_to_plane_dist*point_to_plane_dist;
            }
            return res;
        }
    };

    template <class CorrespondenceSearchEngineT>
    using CombinedMetricRigidICP3f = CombinedMetricRigidICP3<float,CorrespondenceSearchEngineT>;

    template <class CorrespondenceSearchEngineT>
    using CombinedMetricRigidICP3d = CombinedMetricRigidICP3<double,CorrespondenceSearchEngineT>;
}
