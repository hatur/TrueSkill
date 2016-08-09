#pragma once

#include <gsl/gsl.h>

namespace ts {

/**
* Simple lightweight object locker for classes that implement "cl_lock() const" and "cl_unlock() const"
* The classes can implement mutex or recursive mutex
**/
template<typename T>
class conc_lock
{
public:
	explicit conc_lock(const T* obj);

	~conc_lock();

private:
	const T* m_obj;
};

template<typename T>
inline conc_lock<T>::conc_lock(const T* obj)
	: m_obj{obj} {

	Expects(obj != nullptr);

	m_obj->cl_lock();
}

template<typename T>
inline conc_lock<T>::~conc_lock() {
	m_obj->cl_unlock();
}

} // namespace ts
