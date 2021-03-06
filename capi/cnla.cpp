#include <El.h>

#include "nlac.hpp"

#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/json_parser.hpp"

#include "matrix_types.hpp"
#include "../nla/nla.hpp"

skylark::base::context_t &dref_context(sl_context_t *ctxt);

extern "C" {

SKYLARK_EXTERN_API int sl_approximate_symmetric_svd(
    char *A_type, void *A_, char *S_type, void *S_, char *V_type, void *V_,
    uint16_t k, char *params, sl_context_t *ctxt) {

    //lower ? El::LOWER : El:UPPER
    boost::property_tree::ptree json_tree;
    std::stringstream data;
    data << params;
    boost::property_tree::read_json(data, json_tree);
    skylark::nla::approximate_svd_params_t parms(json_tree);

    SKYLARK_BEGIN_TRY()
        skylark::nla::ApproximateSymmetricSVD(El::LOWER,
            skylark_void2any(A_type, A_),
            skylark_void2any(V_type, V_),
            skylark_void2any(S_type, S_),
            k, dref_context(ctxt), parms);
    SKYLARK_END_TRY()
        SKYLARK_CATCH_COPY_AND_RETURN_ERROR_CODE(lastexception);

    return 0;

}

SKYLARK_EXTERN_API int sl_approximate_svd(
    char *A_type, void *A_, char *U_type, void *U_,
    char *S_type, void *S_, char *V_type, void *V_,
    uint16_t k, char *params, sl_context_t *ctxt) {

    boost::property_tree::ptree json_tree;
    std::stringstream data;
    data << params;
    boost::property_tree::read_json(data, json_tree);
    skylark::nla::approximate_svd_params_t parms(json_tree);

    SKYLARK_BEGIN_TRY()
        skylark::nla::ApproximateSVD(skylark_void2any(A_type, A_),
            skylark_void2any(U_type, U_),
            skylark_void2any(S_type, S_),
            skylark_void2any(V_type, V_),
            k, dref_context(ctxt), parms);
    SKYLARK_END_TRY()
        SKYLARK_CATCH_COPY_AND_RETURN_ERROR_CODE(lastexception);

    return 0;
}

SKYLARK_EXTERN_API int sl_faster_least_squares(ElOrientation orientation,
        char *A_type, void *A_, char *B_type, void *B_,
        char *X_type, void *X_, char *params, sl_context_t *ctxt) {

    boost::property_tree::ptree json_tree;
    std::stringstream data;
    data << params;
    boost::property_tree::read_json(data, json_tree);
    skylark::nla::faster_ls_params_t parms(json_tree);

    SKYLARK_BEGIN_TRY()
        skylark::nla::FasterLeastSquares(El::CReflect(orientation),
            skylark_void2any(A_type, A_),
            skylark_void2any(B_type, B_),
            skylark_void2any(X_type, X_),
            dref_context(ctxt), parms);
    SKYLARK_END_TRY()
        SKYLARK_CATCH_COPY_AND_RETURN_ERROR_CODE(lastexception);

    return 0;
}

} // extern "C"
