#pragma once

#include <cilantro/correspondence.hpp>
#include <cilantro/icp_common_feature_adaptors.hpp>
#include <cilantro/image_point_cloud_conversions.hpp>

namespace cilantro {
    template <class ScalarT, class EvaluatorT = CorrespondenceDistanceEvaluator<ScalarT>>
    class ICPCorrespondenceSearchProjective3 {
    public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW

        typedef decltype(std::declval<EvaluatorT>().operator()((size_t)0,(size_t)0,(ScalarT)0)) CorrespondenceScalar;

        typedef CorrespondenceSet<CorrespondenceScalar> SearchResult;

        ICPCorrespondenceSearchProjective3(PointFeaturesAdaptor<ScalarT,3> &dst_points,
                                           PointFeaturesAdaptor<ScalarT,3> &src_points,
                                           EvaluatorT &evaluator)
                : dst_points_adaptor_(dst_points), src_points_adaptor_(src_points), evaluator_(evaluator),
                  projection_image_width_(640), projection_image_height_(480),
                  projection_extrinsics_(RigidTransformation<ScalarT,3>::Identity()),
                  projection_extrinsics_inv_(RigidTransformation<ScalarT,3>::Identity()),
                  max_distance_((CorrespondenceScalar)(0.01*0.01)), inlier_fraction_(1.0)
        {
            // "Kinect"-like defaults
            projection_intrinsics_ << 528, 0, 320, 0, 528, 240, 0, 0, 1;
        }

        template <class TransformT>
        inline ICPCorrespondenceSearchProjective3& findCorrespondences(const TransformT &tform, SearchResult &correspondences) {
            find_correspondences_(src_points_adaptor_.getTransformedFeatureData(tform), correspondences);
            return *this;
        }

        template <class TransformT>
        inline SearchResult findCorrespondences(const TransformT &tform) {
            SearchResult correspondences;
            findCorrespondences<TransformT>(tform, correspondences);
            return correspondences;
        }

        inline ICPCorrespondenceSearchProjective3& findCorrespondences(SearchResult &correspondences) {
            find_correspondences_(src_points_adaptor_.getFeatureData(), correspondences);
            return *this;
        }

        inline SearchResult findCorrespondences() {
            SearchResult correspondences;
            findCorrespondences(correspondences);
            return correspondences;
        }

        inline const Eigen::Matrix<ScalarT,3,3>& getProjectionIntrinsicMatrix() const { return projection_intrinsics_; }

        inline ICPCorrespondenceSearchProjective3& setProjectionIntrinsicMatrix(const Eigen::Ref<const Eigen::Matrix<ScalarT,3,3>> &mat) {
            projection_intrinsics_ = mat;
            index_map_.resize(0,0);
            return *this;
        }

        inline size_t getProjectionImageWidth() const { return projection_image_width_; }

        inline ICPCorrespondenceSearchProjective3& setProjectionImageWidth(size_t w) {
            projection_image_width_ = w;
            index_map_.resize(0,0);
            return *this;
        }

        inline size_t getProjectionImageHeight() const { return projection_image_height_; }

        inline ICPCorrespondenceSearchProjective3& setProjectionImageHeight(size_t h) {
            projection_image_height_ = h;
            index_map_.resize(0,0);
            return *this;
        }

        inline const RigidTransformation<ScalarT,3>& getProjectionExtrinsicMatrix() const {
            return projection_extrinsics_;
        }

        inline ICPCorrespondenceSearchProjective3& setProjectionExtrinsicMatrix(const RigidTransformation<ScalarT,3> &mat) {
            projection_extrinsics_ = mat;
            projection_extrinsics_inv_ = mat.inverse();
            index_map_.resize(0,0);
            return *this;
        }

        inline CorrespondenceScalar getMaxDistance() const { return max_distance_; }

        inline ICPCorrespondenceSearchProjective3& setMaxDistance(CorrespondenceScalar dist_thresh) {
            max_distance_ = dist_thresh;
            return *this;
        }

        inline double getInlierFraction() const { return inlier_fraction_; }

        inline ICPCorrespondenceSearchProjective3& setInlierFraction(double fraction) {
            inlier_fraction_ = fraction;
            return *this;
        }

    private:
        PointFeaturesAdaptor<ScalarT,3>& dst_points_adaptor_;
        PointFeaturesAdaptor<ScalarT,3>& src_points_adaptor_;
        EvaluatorT& evaluator_;

        Eigen::Matrix<size_t,Eigen::Dynamic,Eigen::Dynamic> index_map_;

        Eigen::Matrix<ScalarT,3,3> projection_intrinsics_;
        size_t projection_image_width_;
        size_t projection_image_height_;
        RigidTransformation<ScalarT,3> projection_extrinsics_;
        RigidTransformation<ScalarT,3> projection_extrinsics_inv_;

        CorrespondenceScalar max_distance_;
        double inlier_fraction_;

        void find_correspondences_(const ConstVectorSetMatrixMap<ScalarT,3>& src_points_trans, SearchResult &correspondences) {
            const ConstVectorSetMatrixMap<ScalarT,3>& dst_points(dst_points_adaptor_.getFeatureData());

            if (index_map_.rows() != projection_image_width_ || index_map_.cols() != projection_image_height_) {
                index_map_.resize(projection_image_width_, projection_image_height_);
                pointsToIndexMap<ScalarT>(dst_points, projection_extrinsics_, projection_intrinsics_, index_map_.data(), projection_image_width_, projection_image_height_);
            }

            Vector<ScalarT,3> src_pt_trans_cam;
            const size_t empty = std::numeric_limits<size_t>::max();

            SearchResult corr_tmp(src_points_trans.cols());
            const CorrespondenceScalar value_to_reject = max_distance_ + (CorrespondenceScalar)1.0;
#pragma omp parallel for
            for (size_t i = 0; i < corr_tmp.size(); i++) {
                corr_tmp[i].value = value_to_reject;
            }
#pragma omp parallel for private (src_pt_trans_cam)
            for (size_t i = 0; i < src_points_trans.cols(); i++) {
                src_pt_trans_cam = projection_extrinsics_inv_*src_points_trans.col(i);
                if (src_pt_trans_cam(2) <= (ScalarT)0.0) continue;
                size_t x = (size_t)std::llround(src_pt_trans_cam(0)*projection_intrinsics_(0,0)/src_pt_trans_cam(2) + projection_intrinsics_(0,2));
                size_t y = (size_t)std::llround(src_pt_trans_cam(1)*projection_intrinsics_(1,1)/src_pt_trans_cam(2) + projection_intrinsics_(1,2));
                if (x >= projection_image_width_ || y >= projection_image_height_) continue;
                size_t ind = index_map_(x,y);
                if (ind == empty) continue;
                corr_tmp[i].indexInFirst = ind;
                corr_tmp[i].indexInSecond = i;
                corr_tmp[i].value = evaluator_(ind, i, (src_points_trans.col(i) - dst_points.col(ind)).squaredNorm());
            }

            correspondences.resize(corr_tmp.size());
            size_t count = 0;
            for (size_t i = 0; i < corr_tmp.size(); i++) {
                if (corr_tmp[i].value < max_distance_) correspondences[count++] = corr_tmp[i];
            }
            correspondences.resize(count);

            filterCorrespondencesFraction(correspondences, inlier_fraction_);
        }
    };
}
