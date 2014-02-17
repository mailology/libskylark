#ifndef SKYLARK_RLT_ELEMENTAL_HPP
#define SKYLARK_RLT_ELEMENTAL_HPP

#include <elemental.hpp>

#include "context.hpp"
#include "transforms.hpp"
#include "RLT_data.hpp"
#include "../utility/exception.hpp"


namespace skylark {
namespace sketch {

/**
 * Specialization for local input, local output
 */
template <typename ValueType,
          template <typename> class KernelDistribution>
struct RLT_t <
    elem::Matrix<ValueType>,
    elem::Matrix<ValueType>,
    KernelDistribution> :
        public RLT_data_t<ValueType,
                          KernelDistribution> {
    // Typedef value, matrix, transform, distribution and transform data types
    // so that we can use them regularly and consistently.
    typedef ValueType value_type;
    typedef elem::Matrix<value_type> matrix_type;
    typedef elem::Matrix<value_type> output_matrix_type;
    typedef RLT_data_t<ValueType,
                       KernelDistribution> base_data_t;
private:
    typedef skylark::sketch::dense_transform_t <matrix_type,
                                                output_matrix_type,
                                                KernelDistribution>
    underlying_t;


protected:
    /**
     * Regular constructor - allow creation only by subclasses
     */
    RLT_t (int N, int S, skylark::sketch::context_t& context)
        : base_data_t (N, S, context) {

    }

public:
    /**
     * Copy constructor
     */
    RLT_t(const RFT_t<matrix_type,
                      output_matrix_type,
                      KernelDistribution>& other)
        : base_data_t(other.get_data()) {

    }

    /**
     * Constructor from data
     */
    RLT_t(const base_data_t& other_data)
        : base_data_t(other_data.get_data()) {

    }

    /**
     * Apply the sketching transform that is described in by the sketch_of_A.
     */
    template <typename Dimension>
    void apply (const matrix_type& A,
                output_matrix_type& sketch_of_A,
                Dimension dimension) const {
        try {
            apply_impl(A, sketch_of_A, dimension);
        } catch (std::logic_error e) {
            SKYLARK_THROW_EXCEPTION (
                utility::elemental_exception()
                    << utility::error_msg(e.what()) );
        } catch(boost::mpi::exception e) {
            SKYLARK_THROW_EXCEPTION (
                utility::mpi_exception()
                    << utility::error_msg(e.what()) );
        }
    }

private:
    /**
     * Apply the sketching transform on A and write to sketch_of_A.
     * Implementation for columnwise.
     */
    void apply_impl(const matrix_type& A,
        output_matrix_type& sketch_of_A,
        skylark::sketch::columnwise_tag tag) const {

        underlying_t underlying(base_data_t::_underlying_data);
        underlying.apply(A, sketch_of_A, tag);
        for(int j = 0; j < A.Width(); j++)
            for(int i = 0; i < base_data_t::_S; i++) {
                value_type val = sketch_of_A.Get(i, j);
                sketch_of_A.Set(i, j,
                    base_data_t::_scale * std::exp(- val * base_data_t::_val_scale));
            }
    }

    /**
      * Apply the sketching transform on A and write to  sketch_of_A.
      * Implementation rowwise.
      */
    void apply_impl(const matrix_type& A,
        output_matrix_type& sketch_of_A,
        skylark::sketch::rowwise_tag tag) const {

        // TODO verify sizes etc.
        underlying_t underlying(base_data_t::_underlying_data);
        underlying.apply(A, sketch_of_A, tag);
        for(int j = 0; j < base_data_t::_S; j++)
            for(int i = 0; i < A.Height(); i++) {
                value_type val = sketch_of_A.Get(i, j);
                sketch_of_A.Set(i, j,
                    base_data_t::_scale * std::exp(- val * base_data_t::_val_scale));

            }
    }
};

} } /** namespace skylark::sketch */

#endif // SKYLARK_RLT_ELEMENTAL_HPP