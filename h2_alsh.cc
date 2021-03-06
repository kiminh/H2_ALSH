#include <algorithm>

#include "def.h"
#include "util.h"
#include "pri_queue.h"
#include "qalsh.h"
#include "h2_alsh.h"

// -----------------------------------------------------------------------------
H2_ALSH::H2_ALSH(					// constructor
	int   n,							// number of data objects
	int   d,							// dimension of data objects
	float nn_ratio,						// approximation ratio for ANN search
	float mip_ratio,					// approximation ratio for AMIP search
	const float **data, 				// input data
	const float **norm_d)				// l2-norm of data objects
{
	// -------------------------------------------------------------------------
	//  init parameters
	// -------------------------------------------------------------------------
	n_pts_     = n;	
	dim_       = d;
	nn_ratio_  = nn_ratio;
	mip_ratio_ = mip_ratio;
	data_      = data;
	norm_d_	   = norm_d;

	// -------------------------------------------------------------------------
	//  build index
	// -------------------------------------------------------------------------
	bulkload();
}

// -----------------------------------------------------------------------------
H2_ALSH::~H2_ALSH()					// destructor
{
	for (int i = 0; i < n_pts_; ++i) {
		delete[] h2_alsh_data_[i]; h2_alsh_data_[i] = NULL;
	}
	delete[] h2_alsh_data_; h2_alsh_data_ = NULL;

	for (int i = 0; i < num_blocks_; ++i) {
		delete blocks_[i]; blocks_[i] = NULL;
	}
	blocks_.clear(); blocks_.shrink_to_fit();
}

// -----------------------------------------------------------------------------
void H2_ALSH::bulkload()			// bulkloading
{
	// -------------------------------------------------------------------------
	//  sort data objects by their Euclidean norms under the ascending order
	// -------------------------------------------------------------------------
	Result *order = new Result[n_pts_];
	for (int i = 0; i < n_pts_; ++i) {
		order[i].key_ = norm_d_[i][0];
		order[i].id_  = i;			// data object id		
	}
	qsort(order, n_pts_, sizeof(Result), ResultCompDesc);

	M_ = order[0].key_;
	b_ = sqrt((pow(nn_ratio_,4.0f) - 1) / (pow(nn_ratio_,4.0f) - mip_ratio_));

	// -------------------------------------------------------------------------
	//  construct new data
	// -------------------------------------------------------------------------
	h2_alsh_data_ = new float*[n_pts_];
	num_blocks_ = 0;

	int i = 0;
	while (i < n_pts_) {
		int   n     = 0;
		float M     = order[i].key_;
		float m     = M * b_;
		float M_sqr = M * M;

		while (i < n_pts_) {
			if (n >= MAX_BLOCK_NUM) break;

			float norm_d = order[i].key_;
			if (norm_d < m) break;

			int id = order[i].id_;
			float *data = new float[dim_ + 1];
			for (int j = 0; j < dim_; ++j) {
				data[j] = data_[id][j];
			}
			data[dim_]= sqrt(M_sqr - norm_d * norm_d);
			h2_alsh_data_[i] = data; 
			++i; ++n;
		}

		Block *block = new Block();
		block->n_pts_ = n;
		block->M_     = M;

		int *index = new int[n];
		Result *tmp = &order[i - n];
		for (int j = 0; j < n; ++j) {
			index[j] = tmp[j].id_;
		}
		block->index_ = index;

		if (n > N_THRESHOLD) {
			int start = i - n;
			block->lsh_ = new QALSH(n, dim_ + 1, nn_ratio_, 
				(const float **) h2_alsh_data_ + start);
		}
		blocks_.push_back(block);
		++num_blocks_;
	}
	delete[] order; order = NULL;
}

// -------------------------------------------------------------------------
void H2_ALSH::display()				// display parameters
{
	printf("Parameters of H2_ALSH:\n");
	printf("    n          = %d\n",   n_pts_);
	printf("    d          = %d\n",   dim_);
	printf("    c0         = %.1f\n", nn_ratio_);
	printf("    c          = %.1f\n", mip_ratio_);
	printf("    M          = %f\n",   M_);
	printf("    num_blocks = %d\n\n", num_blocks_);
}

// -----------------------------------------------------------------------------
int H2_ALSH::kmip(					// c-k-AMIP search
	int   top_k,						// top-k value
	const float *query,					// input query
	const float *norm_q,				// l2-norm of query
	MaxK_List *list)					// top-k MIP results (return)  
{
	// -------------------------------------------------------------------------
	//  initialize parameters
	// -------------------------------------------------------------------------
	float kip   = MINREAL;
	float normq = norm_q[0];
	float *h2_alsh_query = new float[dim_ + 1];
	std::vector<int> cand;

	// -------------------------------------------------------------------------
	//  c-k-AMIP search
	// -------------------------------------------------------------------------
	for (int i = 0; i < num_blocks_; ++i) {
		Block *block = blocks_[i];
		int   *index = block->index_;
		int   n      = block->n_pts_;
		float M      = block->M_;
		if (M * normq <= kip) break;

		if (n <= N_THRESHOLD) {
			// -----------------------------------------------------------------
			//  MIP search by linear scan
			// -----------------------------------------------------------------
			for (int j = 0; j < n; ++j) {
				int id = index[j];
				if (norm_d_[id][0] * normq <= kip) break;
				
				float ip = calc_inner_product(dim_, kip, data_[id], 
					norm_d_[id], query, norm_q);
				kip = list->insert(ip, id + 1);
			}
		}
		else {
			// -----------------------------------------------------------------
			//  conduct c-k-ANN search by qalsh
			// -----------------------------------------------------------------
			float lambda = M / normq;
			float R = sqrt(2.0f * (M * M - lambda * kip));
			for (int j = 0; j < dim_; ++j) {
				h2_alsh_query[j] = lambda * query[j];
			}
			h2_alsh_query[dim_] = 0.0f;

			cand.clear();
			block->lsh_->knn(top_k, R, (const float *) h2_alsh_query, cand);

			// -----------------------------------------------------------------
			//  compute inner product for the candidates returned by qalsh
			// -----------------------------------------------------------------
			int size = (int) cand.size();
			for (int j = 0; j < size; ++j) {
				int id = index[cand[j]];

				if (norm_d_[id][0] * normq > kip) {
					float ip = calc_inner_product(dim_, kip, data_[id], 
						norm_d_[id], query, norm_q);
					kip = list->insert(ip, id + 1);
				}
			}
		}
	}
	delete[] h2_alsh_query; h2_alsh_query = NULL;

	return 0;
}
