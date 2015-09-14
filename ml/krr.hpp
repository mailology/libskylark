#ifndef SKYLARK_KRR_HPP
#define SKYLARK_KRR_HPP

namespace skylark { namespace ml {

struct krr_params_t : public base::params_t {

    // For approximate methods (ApproximateKRR)
    bool sketched_rr;
    El::Int sketch_size;
    bool fast_sketch;

    // For iterative methods (FasterKRR)
    int iter_lim;
    int res_print;
    double tolerance;

    krr_params_t(bool am_i_printing = 0,
        int log_level = 0,
        std::ostream &log_stream = std::cout,
        std::string prefix = "",
        int debug_level = 0) :
        base::params_t(am_i_printing, log_level, log_stream, prefix, debug_level) {

        sketched_rr = false;
        sketch_size = -1;
        fast_sketch = false;

        tolerance = 1e-3;
        res_print = 10;
        iter_lim = 1000;
  }

};

template<typename T, typename KernelType>
void KernelRidge(base::direction_t direction, const KernelType &k,
    const El::DistMatrix<T> &X, const El::DistMatrix<T> &Y, T lambda,
    El::DistMatrix<T> &A, krr_params_t params = krr_params_t()) {

    bool log_lev1 = params.am_i_printing && params.log_level >= 1;
    bool log_lev2 = params.am_i_printing && params.log_level >= 2;

    boost::mpi::timer timer;

    // Compute kernel matrix
    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Computing kernel matrix... ";
        params.log_stream.flush();
        timer.restart();
    }

    El::DistMatrix<T> K;
    SymmetricGram(El::LOWER, direction, k, X, K);

    // Add regularizer
    El::DistMatrix<T> D;
    El::Ones(D, X.Width(), 1);
    El::UpdateDiagonal(K, lambda, D);

    if (log_lev1)
        params.log_stream << "took " << boost::format("%.2e") % timer.elapsed()
                          << " sec\n";

    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Solving the equation... ";
        params.log_stream.flush();
        timer.restart();
    }

    A = Y;
    HPDSolve(El::LOWER, El::NORMAL, K, A);

    if (log_lev1)
        params.log_stream << "took " << boost::format("%.2e") % timer.elapsed()
                          << " sec\n";
}

template<typename T, typename KernelType>
void ApproximateKernelRidge(base::direction_t direction, const KernelType &k,
    const El::DistMatrix<T> &X, const El::DistMatrix<T> &Y, T lambda,
    sketch::sketch_transform_container_t<El::DistMatrix<T>, El::DistMatrix<T> > &S,
    El::DistMatrix<T> &W, El::Int s, base::context_t &context,
    krr_params_t params = krr_params_t()) {

    bool log_lev1 = params.am_i_printing && params.log_level >= 1;
    bool log_lev2 = params.am_i_printing && params.log_level >= 2;

    boost::mpi::timer timer;

    // Create and apply the feature transform
    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Create and apply the feature transform... ";
        params.log_stream.flush();
        timer.restart();
   }

    sketch::generic_sketch_transform_ptr_t
        p(k.create_rft(s, regular_feature_transform_tag(), context));
    S =
        sketch::sketch_transform_container_t<El::DistMatrix<T>,
                                             El::DistMatrix<T> >(p);

    El::DistMatrix<T> Z;

    if (direction == base::COLUMNS) {
        Z.Resize(s, X.Width());
        S.apply(X, Z, sketch::columnwise_tag());
    } else {
        Z.Resize(X.Height(), s);
        S.apply(X, Z, sketch::rowwise_tag());
    }

    if (log_lev1)
        params.log_stream << "took " << boost::format("%.2e") % timer.elapsed()
                          << " sec\n";

    // Sketch the problem (if requested)
    El::DistMatrix<T> SZ, SY;
    if (params.sketched_rr) {

        if (log_lev1) {
            params.log_stream << params.prefix
                              << "Sketching the regression problem... ";
            params.log_stream.flush();
            timer.restart();
        }

        El::Int m = direction ==  base::COLUMNS ? Z.Width() : Z.Height();
        El::Int t = params.sketch_size == -1 ? 4 * s : params.sketch_size;

        sketch::sketch_transform_t<El::DistMatrix<T>, El::DistMatrix<T> > *R;
        if (params.fast_sketch)
            R = new sketch::CWT_t<El::DistMatrix<T>,
                                  El::DistMatrix<T> >(m, t, context);
        else
            R = new sketch::FJLT_t<El::DistMatrix<T>,
                                   El::DistMatrix<T> >(m, t, context);

        if (direction == base::COLUMNS) {
            SZ.Resize(s, t);
            R->apply(Z, SZ, sketch::rowwise_tag());

            // TODO it is "wrong" that Y is oriented differently than X/Z
            SY.Resize(t, Y.Width());
            R->apply(Y, SY, sketch::columnwise_tag());

        } else {
            SZ.Resize(t, s);
            R->apply(Z, SZ, sketch::columnwise_tag());
            SY.Resize(t, Y.Width());
            R->apply(Y, SY, sketch::columnwise_tag());
        }

        delete R;

        if (log_lev1)
            params.log_stream << "took " << boost::format("%.2e") % timer.elapsed()
                              << " sec\n";
    } else {
        El::View(SZ, Z);
        El::LockedView(SY, Y);
    }

    // Solving the regression problem
    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Solving the regression problem... ";
        params.log_stream.flush();
            timer.restart();
    }

    El::Ridge(direction == base::COLUMNS ? El::ADJOINT : El::NORMAL,
        SZ, SY, std::sqrt(lambda), W);

    if (log_lev1)
        params.log_stream << "took " << boost::format("%.2e") % timer.elapsed()
                          << " sec\n";
}

template<typename MatrixType>
class feature_map_precond_t :
    public algorithms::outplace_precond_t<MatrixType, MatrixType> {

public:

    typedef MatrixType matrix_type;
    typedef typename utility::typer_t<matrix_type>::value_type value_type;

    virtual bool is_id() const { return false; }

    template<typename KernelType, typename InputType>
    feature_map_precond_t(const KernelType &k, value_type lambda,
        const InputType &X, El::Int s, base::context_t &context,
        const krr_params_t &params) {
        _lambda = lambda;
        _s = s;

        bool log_lev2 = params.am_i_printing && params.log_level >= 2;

        boost::mpi::timer timer;

        if (log_lev2) {
            params.log_stream << params.prefix << "\t"
                              << "Applying random features transform... ";
            params.log_stream.flush();
            timer.restart();
        }

        U.Resize(s, X.Width());
        sketch::sketch_transform_t<InputType, matrix_type> *S =
            k.template create_rft<InputType, matrix_type>(s,
                ml::regular_feature_transform_tag(),
                context);
        S->apply(X, U, sketch::columnwise_tag());
        delete S;

        if (log_lev2)
            params.log_stream << "took " << boost::format("%.2e") % timer.elapsed()
                              << " sec\n";

        if (log_lev2) {
            params.log_stream << params.prefix << "\t"
                              << "Computing covariance matrix... ";
            params.log_stream.flush();
            timer.restart();
        }

        El::Identity(C, s, s);
        El::Herk(El::LOWER, El::NORMAL, value_type(1.0)/_lambda, U,
            value_type(1.0), C);

        if (log_lev2)
            params.log_stream << "took " << boost::format("%.2e") % timer.elapsed()
                              << " sec\n";

        if (log_lev2) {
            params.log_stream << params.prefix << "\t"
                              << "Factorizing... ";
            params.log_stream.flush();
            timer.restart();
        }


        El::Cholesky(El::LOWER, C);

        if (log_lev2)
            params.log_stream << "took " << boost::format("%.2e") % timer.elapsed()
                              << " sec\n";
    }

    virtual void apply(const matrix_type& B, matrix_type& X) const {

        matrix_type CUB(_s, B.Width());
        El::Gemm(El::NORMAL, El::NORMAL, value_type(1.0), U, B, CUB);
        El::cholesky::SolveAfter(El::LOWER, El::NORMAL, C, CUB);

        X = B;
        El::Gemm(El::ADJOINT, El::NORMAL, value_type(-1.0) / (_lambda * _lambda), 
            U, CUB, value_type(1.0)/_lambda, X);
    }

    virtual void apply_adjoint(const matrix_type& B, matrix_type& X) const {
        apply(B, X);
    }

private:
    value_type _lambda;
    El::Int _s;
    matrix_type U, C;
};

template<typename T, typename KernelType>
void FasterKernelRidge(base::direction_t direction, const KernelType &k,
    const El::DistMatrix<T> &X, const El::DistMatrix<T> &Y, T lambda,
    El::DistMatrix<T> &A, El::Int s, base::context_t &context,
    krr_params_t params = krr_params_t()) {

    bool log_lev1 = params.am_i_printing && params.log_level >= 1;
    bool log_lev2 = params.am_i_printing && params.log_level >= 2;

    boost::mpi::timer timer;

    // Compute kernel matrix
    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Computing kernel matrix... ";
        params.log_stream.flush();
        timer.restart();
    }

    El::DistMatrix<T> K;
    SymmetricGram(El::LOWER, direction, k, X, K);

    // Add regularizer
    El::DistMatrix<T> D;
    El::Ones(D, X.Width(), 1);
    El::UpdateDiagonal(K, lambda, D);

    if (log_lev1)
        params.log_stream << "took " << boost::format("%.2e") % timer.elapsed()
                          << " sec\n";

    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Creating precoditioner... ";
        if (log_lev2)
            params.log_stream << std::endl;
        params.log_stream.flush();
        timer.restart();
    }

    feature_map_precond_t<El::DistMatrix<T> > P(k, lambda, X, s, context, params);

    if (log_lev1 && !log_lev2)
        params.log_stream << "took " << boost::format("%.2e") % timer.elapsed()
                          << " sec\n";

    if (log_lev2)
        params.log_stream << params.prefix
                          << "Took " << boost::format("%.2e") % timer.elapsed()
                          << " sec\n";

    if (log_lev1) {
        params.log_stream << params.prefix
                          << "Solving linear equation... "
                          << std::endl;
        params.log_stream.flush();
        timer.restart();
    }


    // Solve
    algorithms::krylov_iter_params_t cg_params(params.tolerance, params.iter_lim,
        params.am_i_printing, params.log_level - 1, params.res_print, 
        params.log_stream, params.prefix + "\t");

    El::Zeros(A, X.Width(), Y.Width());
    algorithms::CG(El::LOWER, K, Y, A, cg_params, P);

    if (log_lev1)
        params.log_stream  << params.prefix
                           <<"Took " << boost::format("%.2e") % timer.elapsed()
                           << " sec\n";

}

} } // namespace skylark::ml

#endif
