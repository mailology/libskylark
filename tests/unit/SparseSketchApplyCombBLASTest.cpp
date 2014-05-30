/**
 *  This test ensures that the sketch application (for CombBLAS matrices) is
 *  done correctly (on-the-fly matrix multiplication in the code is compared
 *  to true matrix multiplication).
 *  This test builds on the following assumptions:
 *
 *      - CombBLAS PSpGEMM returns the correct result, and
 *      - the random numbers in row_idx and row_value (see
 *        hash_transform_data_t) are drawn from the promised distributions.
 */


#include <vector>

#include <boost/mpi.hpp>
#include <boost/test/minimal.hpp>

#include "../../utility/distributions.hpp"
#include "../../base/context.hpp"
#include "../../sketch/hash_transform.hpp"

#include "../../base/sparse_matrix.hpp"

typedef FullyDistVec<size_t, double> mpi_vector_t;
typedef SpDCCols<size_t, double> col_t;
typedef SpParMat<size_t, double, col_t> DistMatrixType;
typedef PlusTimesSRing<double, double> PTDD;
typedef skylark::base::sparse_matrix_t<double> LocalMatrixType;


template < typename InputMatrixType,
           typename OutputMatrixType = InputMatrixType >
struct Dummy_t : public skylark::sketch::hash_transform_t<
    InputMatrixType, OutputMatrixType,
    boost::random::uniform_int_distribution,
    skylark::utility::rademacher_distribution_t > {

    typedef skylark::sketch::hash_transform_t<
        InputMatrixType, OutputMatrixType,
        boost::random::uniform_int_distribution,
        skylark::utility::rademacher_distribution_t >
            hash_t;

    Dummy_t(int N, int S, skylark::base::context_t& context)
        : skylark::sketch::hash_transform_t<InputMatrixType, OutputMatrixType,
          boost::random::uniform_int_distribution,
          skylark::utility::rademacher_distribution_t>(N, S, context)
    {}

    std::vector<size_t> getRowIdx() { return hash_t::row_idx; }
    std::vector<double> getRowValues() { return hash_t::row_value; }
};


template<typename sketch_t>
void compute_sketch_matrix(sketch_t sketch, const DistMatrixType &A,
                           DistMatrixType &result) {

    std::vector<size_t> row_idx = sketch.getRowIdx();
    std::vector<double> row_val = sketch.getRowValues();

    // PI generated by random number gen
    size_t sketch_size = row_val.size();
    mpi_vector_t cols(sketch_size);
    mpi_vector_t rows(sketch_size);
    mpi_vector_t vals(sketch_size);

    for(size_t i = 0; i < sketch_size; ++i) {
        cols.SetElement(i, i);
        rows.SetElement(i, row_idx[i]);
        vals.SetElement(i, row_val[i]);
    }

    result = DistMatrixType(result.getnrow(), result.getncol(),
                            rows, cols, vals);
}


int test_main(int argc, char *argv[]) {

    //////////////////////////////////////////////////////////////////////////
    //[> Parameters <]

    //FIXME: use random sizes?
    const size_t n   = 200;
    const size_t m   = 100;
    const size_t n_s = 120;
    const size_t m_s = 60;

    //////////////////////////////////////////////////////////////////////////
    //[> Setup test <]
    namespace mpi = boost::mpi;
    mpi::environment env(argc, argv);
    mpi::communicator world;
    const size_t rank = world.rank();

    skylark::base::context_t context (0);

    double count = 1.0;

    const size_t matrix_full = n * m;
    mpi_vector_t colsf(matrix_full);
    mpi_vector_t rowsf(matrix_full);
    mpi_vector_t valsf(matrix_full);

    for(size_t i = 0; i < matrix_full; ++i) {
        colsf.SetElement(i, i % m);
        rowsf.SetElement(i, i / m);
        valsf.SetElement(i, count);
        count++;
    }

    DistMatrixType A(n, m, rowsf, colsf, valsf);


    //////////////////////////////////////////////////////////////////////////
    //[> Column wise application DistSparseMatrix -> DistSparseMatrix <]

    //[> 1. Create the sketching matrix <]
    Dummy_t<DistMatrixType, DistMatrixType> Sparse(n, n_s, context);

    //[> 2. Create space for the sketched matrix <]
    mpi_vector_t zero;
    DistMatrixType sketch_A(n_s, m, zero, zero, zero);

    //[> 3. Apply the transform <]
    Sparse.apply(A, sketch_A, skylark::sketch::columnwise_tag());

    //[> 4. Build structure to compare <]
    DistMatrixType pi_sketch(n_s, n, zero, zero, zero);
    compute_sketch_matrix(Sparse, A, pi_sketch);
    DistMatrixType expected_A = Mult_AnXBn_Synch<PTDD, double, col_t>(pi_sketch, A, false, false);
    if (!static_cast<bool>(expected_A == sketch_A))
        BOOST_FAIL("Result of colwise (dist -> dist) application not as expected");

    //////////////////////////////////////////////////////////////////////////
    //[> Column wise application DistSparseMatrix -> LocalSparseMatrix <]

    Dummy_t<DistMatrixType, LocalMatrixType> LocalSparse(n, n_s, context);
    LocalMatrixType local_sketch_A;
    LocalSparse.apply(A, local_sketch_A, skylark::sketch::columnwise_tag());

    if(rank == 0) {
        std::vector<size_t> row_idx = LocalSparse.getRowIdx();
        std::vector<double> row_val = LocalSparse.getRowValues();

        // PI generated by random number gen
        int sketch_size = row_val.size();
        typename LocalMatrixType::coords_t coords;
        for(int i = 0; i < sketch_size; ++i) {
            typename LocalMatrixType::coord_tuple_t new_entry(row_idx[i], i, row_val[i]);
            coords.push_back(new_entry);
        }

        LocalMatrixType pi_sketch_l;
        pi_sketch_l.set(coords);

        typename LocalMatrixType::coords_t coords_new;
        const int* indptr = pi_sketch_l.indptr();
        const int* indices = pi_sketch_l.indices();
        const double* values = pi_sketch_l.locked_values();

        // multiply with vector where an entry has the value:
        //   col_idx + row_idx * m + 1.
        // See creation of A.
        for(int col = 0; col < pi_sketch_l.width(); col++) {
            for(int idx = indptr[col]; idx < indptr[col + 1]; idx++) {
                for(int ccol = 0; ccol < m; ++ccol) {
                    typename LocalMatrixType::coord_tuple_t new_entry(indices[idx],
                            ccol, values[idx] * (ccol + col * m + 1));
                    coords_new.push_back(new_entry);
                }
            }
        }

        LocalMatrixType expected_A_l;
        expected_A_l.set(coords_new, n_s, m);

        if (!static_cast<bool>(expected_A_l == local_sketch_A))
            BOOST_FAIL("Result of local colwise application not as expected");
    }


    //////////////////////////////////////////////////////////////////////////
    //[> Row wise application DistSparseMatrix -> DistSparseMatrix <]

    //[> 1. Create the sketching matrix <]
    Dummy_t<DistMatrixType, DistMatrixType> Sparse_r(m, m_s, context);

    //[> 2. Create space for the sketched matrix <]
    DistMatrixType sketch_A_r(n, m_s, zero, zero, zero);

    //[> 3. Apply the transform <]
    Sparse_r.apply(A, sketch_A_r, skylark::sketch::rowwise_tag());

    //[> 4. Build structure to compare <]
    DistMatrixType pi_sketch_r(m_s, m, zero, zero, zero);
    compute_sketch_matrix(Sparse_r, A, pi_sketch_r);
    pi_sketch_r.Transpose();
    DistMatrixType expected_AR = PSpGEMM<PTDD>(A, pi_sketch_r);

    if (!static_cast<bool>(expected_AR == sketch_A_r))
        BOOST_FAIL("Result of rowwise (dist -> dist) application not as expected");

    return 0;
}
