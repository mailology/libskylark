#ifndef SKYLARK_RFT_ELEMENTAL_HPP
#define SKYLARK_RFT_ELEMENTAL_HPP

#include <elemental.hpp>

#include "context.hpp"
#include "transforms.hpp"
#include "RFT_data.hpp"
#include "../utility/exception.hpp"


namespace skylark {
namespace sketch {

/**
 * Specialization distributed input and output in [*, SOMETHING]
 */
template <typename ValueType,
          elem::Distribution ColDist,
          template <typename> class UnderlyingValueDistribution>
struct RFT_t <
    elem::DistMatrix<ValueType, ColDist, elem::STAR>,
    elem::DistMatrix<ValueType, ColDist, elem::STAR>,
    UnderlyingValueDistribution> :
        public RFT_data_t<ValueType,
                          UnderlyingValueDistribution> {
    // Typedef value, matrix, transform, distribution and transform data types
    // so that we can use them regularly and consistently.
    typedef ValueType value_type;
    typedef boost::random::uniform_real_distribution<> value_distribution_type;
    typedef elem::DistMatrix<value_type, ColDist, elem::STAR> matrix_type;
    typedef elem::DistMatrix<value_type,
                             ColDist, elem::STAR> output_matrix_type;
    typedef RFT_data_t<ValueType,
                       UnderlyingValueDistribution> base_data_t;
private:
    typedef skylark::sketch::dense_transform_t <matrix_type,
                                                output_matrix_type,
                                                UnderlyingValueDistribution>
    underlying_type;


public:
    /**
     * Regular constructor
     */
    RFT_t (int N, int S, double sigma, skylark::sketch::context_t& context)
        : base_data_t (N, S, sigma, context) {}

    /**
     * Copy constructor
     */
    RFT_t(RFT_t<matrix_type,
                output_matrix_type,
                UnderlyingValueDistribution>& other)
        : base_data_t(other.get_data()) {}

    /**
     * Apply the sketching transform that is described in by the sketch_of_A.
     */
    template <typename Dimension>
    void apply (const matrix_type& A,
                output_matrix_type& sketch_of_A,
                Dimension dimension) const {

        switch(ColDist) {
        case elem::VR:
        case elem::VC:
            try {
            apply_impl_vdist (A, sketch_of_A, dimension);
            } catch (std::logic_error e) {
                SKYLARK_THROW_EXCEPTION (
                    utility::elemental_exception()
                        << utility::error_msg(e.what()) );
            } catch(boost::mpi::exception e) {
                SKYLARK_THROW_EXCEPTION (
                    utility::mpi_exception()
                        << utility::error_msg(e.what()) );
            }

            break;

        default:
            SKYLARK_THROW_EXCEPTION (
                utility::unsupported_matrix_distribution() );
        }
    }

private:
    /**
     * Apply the sketching transform that is described in by the sketch_of_A.
     * Implementation for [VR/VC, *] and columnwise.
     */
    void apply_impl_vdist (const matrix_type& A,
                     output_matrix_type& sketch_of_A,
                     skylark::sketch::columnwise_tag tag) const {
        underlying_type underlying(base_data_t::underlying_data);
        underlying.apply(A, sketch_of_A, tag);
        elem::Matrix<value_type> &Al = sketch_of_A.Matrix();
        for(int j = 0; j < Al.Width(); j++)
            for(int i = 0; i < base_data_t::S; i++) {
                value_type val = Al.Get(i, j);
                value_type trans =
                    base_data_t::scale * std::cos((val / base_data_t::sigma) +
                        base_data_t::shifts[i]);
                Al.Set(i, j, trans);
            }
    }

    /**
      * Apply the sketching transform that is described in by the sketch_of_A.
      * Implementation for [VR/VC, *] and rowwise.
      */
    void apply_impl_vdist(const matrix_type& A,
        output_matrix_type& sketch_of_A,
        skylark::sketch::rowwise_tag tag) const {

        // TODO verify sizes etc.
        underlying_type underlying(base_data_t::underlying_data);
        underlying.apply(A, sketch_of_A, tag);
        elem::Matrix<value_type> &Al = sketch_of_A.Matrix();
        for(int j = 0; j < base_data_t::S; j++)
            for(int i = 0; i < Al.Height(); i++) {
                value_type val = Al.Get(i, j);
                value_type trans =
                    base_data_t::scale * std::cos((val / base_data_t::sigma) +
                        base_data_t::shifts[j]);
                Al.Set(i, j, trans);
            }
    }

};

} } /** namespace skylark::sketch */

#endif // SKYLARK_RFT_ELEMENTAL_HPP
