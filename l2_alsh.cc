#include <algorithm>

#include "def.h"
#include "util.h"
#include "pri_queue.h"
#include "qalsh.h"
#include "l2_alsh.h"

// -----------------------------------------------------------------------------
L2_ALSH::L2_ALSH(					// constructor
	int   n,							// number of data objects
	int   d,							// dimension of data objects
	int   m,							// additional dimension of data
	float U,							// scale factor for data
	float nn_ratio,						// approximation ratio for ANN search
	const float **data, 				// input data
	const float **norm_d)				// l2-norm of data objects
{
	// -------------------------------------------------------------------------
	//  init parameters
	// -------------------------------------------------------------------------
	n_pts_       = n;
	dim_         = d;
	m_           = m;
	U_           = U;
	nn_ratio_    = nn_ratio;
	data_        = data;
	norm_d_      = norm_d;
	l2_alsh_dim_ = d + m;

	// -------------------------------------------------------------------------
	//  build index
	// -------------------------------------------------------------------------
	bulkload();
}

// -----------------------------------------------------------------------------
L2_ALSH::~L2_ALSH()					// destructor
{
	delete lsh_; lsh_ = NULL;
	for (int i = 0; i < n_pts_; ++i) {
		delete[] l2_alsh_data_[i]; l2_alsh_data_[i] = NULL;
	}
	delete[] l2_alsh_data_; l2_alsh_data_ = NULL;
}

// -----------------------------------------------------------------------------
void L2_ALSH::bulkload()			// bulkloading
{
	// -------------------------------------------------------------------------
	//  calculate the Euclidean norm of data and find the maximum norm
	// -------------------------------------------------------------------------
	std::vector<float> norm(n_pts_, 0.0f);
	M_ = MINREAL;
	for (int i = 0; i < n_pts_; ++i) {
		norm[i] = norm_d_[i][0];
		if (norm[i] > M_) M_ = norm[i];
	}

	// -------------------------------------------------------------------------
	//  construct new format of data and indexing
	// -------------------------------------------------------------------------
	float scale    = U_ / M_;
	int   exponent = -1;

	l2_alsh_data_ = new float*[n_pts_];
	for (int i = 0; i < n_pts_; ++i) {
		l2_alsh_data_[i] = new float[l2_alsh_dim_];

		norm[i] *= scale;
		for (int j = 0; j < l2_alsh_dim_; ++j) {
			if (j < dim_) {
				l2_alsh_data_[i][j] = data_[i][j] * scale;
			}
			else {
				exponent = (int) pow(2.0f, j - dim_ + 1);
				l2_alsh_data_[i][j] = pow(norm[i], exponent);
			}
		}
	}

	// -------------------------------------------------------------------------
	//  indexing the new format of data using qalsh
	// -------------------------------------------------------------------------
	lsh_ = new QALSH(n_pts_, l2_alsh_dim_, nn_ratio_, (const float **) l2_alsh_data_);
}

// -----------------------------------------------------------------------------
void L2_ALSH::display()				// display parameters
{
	printf("Parameters of L2_ALSH:\n");
	printf("    n  = %d\n",   n_pts_);
	printf("    d  = %d\n",   dim_);
	printf("    m  = %d\n",   m_);
	printf("    U  = %.2f\n", U_);
	printf("    c0 = %.1f\n", nn_ratio_);
	printf("    M  = %f\n\n", M_);
}

// -----------------------------------------------------------------------------
int L2_ALSH::kmip(					// c-k-AMIP search
	int   top_k,						// top-k value
	const float *query,					// input query
	const float *norm_q,				// l2-norm of query
	MaxK_List *list)					// top-k MIP results (return) 
{
	float kip   = MINREAL;
	float normq = norm_q[0];

	// -------------------------------------------------------------------------
	//  construct L2_ALSH query
	// -------------------------------------------------------------------------
	float *l2_alsh_query = new float[l2_alsh_dim_];
	for (int i = 0; i < l2_alsh_dim_; ++i) {
		if (i < dim_) l2_alsh_query[i] = query[i] / normq;
		else l2_alsh_query[i] = 0.5f;
	}

	// -------------------------------------------------------------------------
	//  conduct c-k-ANN search by qalsh
	// -------------------------------------------------------------------------
	std::vector<int> cand;
	lsh_->knn(top_k, MAXREAL, (const float *) l2_alsh_query, cand);

	// -------------------------------------------------------------------------
	//  compute inner product for candidates returned by qalsh
	// -------------------------------------------------------------------------
	int size = (int) cand.size();
	for (int i = 0; i < size; ++i) {
		int id = cand[i];
		if (norm_d_[id][0] * normq <= kip) break;
			
		float ip = calc_inner_product(dim_, kip, data_[id], norm_d_[id], 
			query, norm_q);
		kip = list->insert(ip, id + 1);
	}
	delete[] l2_alsh_query; l2_alsh_query = NULL;

	return 0;
}

