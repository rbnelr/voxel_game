#include <vector>

namespace kiss {
	// returns lowest index where are_equal(vec[i], r) returns true or -1 if none are found
	// bool are_equal(VT const& l, T const& r)
	template <typename VT, typename T, typename EQUAL>
	int indexof (std::vector<VT>& vec, T& r, EQUAL are_equal) {
		for (int i=0; i<(int)vec.size(); ++i)
			if (are_equal(vec[i], r))
				return i;
		return -1;
	}

	// returns lowest index where (vec[i] == r) returns true or -1 if none are found
	template <typename VT, typename T>
	int indexof (std::vector<VT>& vec, T& r) {
		for (int i=0; i<(int)vec.size(); ++i)
			if (vec[i] == r)
				return i;
		return -1;
	}

	// returns true if any are_equal(vec[i], r) returns true
	// bool are_equal(VT const& l, T const& r)
	template <typename VT, typename T, typename EQUAL>
	bool contains (std::vector<VT>& vec, T& r, EQUAL are_equal) {
		for (auto& i : vec)
			if (are_equal(i, r))
				return true;
		return false;
	}
	// returns true if any (vec[i] == r) returns true
	template <typename VT, typename T>
	bool contains (std::vector<VT>& vec, T& r) {
		for (auto& i : vec)
			if (i == r)
				return true;
		return false;
	}
}
