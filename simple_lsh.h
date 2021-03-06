#ifndef __SIMPLE_LSH_H
#define __SIMPLE_LSH_H

class SRP_LSH;
class MaxK_List;

// -----------------------------------------------------------------------------
//  Simple-LSH is used to solve the problem of c-Approximate Maximum Inner 
//  Product (c-AMIP) search.
//
//  the idea was introduced by Behnam Neyshabur and Nathan Srebro in their 
//  paper "On Symmetric and Asymmetric LSHs for Inner Product Search", In 
//  Proceedings of the 32nd International Conference on International Conference 
//  on Machine Learning (ICML), pages 1926–1934, 2015.
// -----------------------------------------------------------------------------
class Simple_LSH {
public:
	Simple_LSH(						// constructor
		int   n,						// number of data objects
		int   d,						// dimensionality
		int   K,						// number of hash tables
		const float **data, 			// input data
		const float **norm_d);			// l2-norm of data objects

	// -------------------------------------------------------------------------
	~Simple_LSH();					// destructor

	// -------------------------------------------------------------------------
	void display();					// display parameters

	// -------------------------------------------------------------------------
	int kmip(						// c-k-AMIP search
		int   top_k,					// top-k value
		const float *query,				// input query
		const float *norm_q,			// l2-norm of query
		MaxK_List *list);				// top-k mip results

protected:
	int   n_pts_;					// number of data objects
	int   dim_;						// dimensionality
	int   K_;						// number of hash tables
	const float **data_;			// original data objects
	const float **norm_d_;			// l2-norm of data objects
	
	float M_;						// max l2-norm of data objects
	float **simple_lsh_data_;		// simple_lsh data
	SRP_LSH *lsh_;					// SRP_LSH

	// -------------------------------------------------------------------------
	void bulkload();				// bulkloading
};

#endif // __SIMPLE_LSH_H
