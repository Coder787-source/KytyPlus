#ifndef KYTY_COMMON_SINGLETON_H_
#define KYTY_COMMON_SINGLETON_H_

#include <cstdlib>
#include <mutex>
#include <new>

namespace Common {

template <class T>
class Singleton {
public:
	static T* Instance() {
		static T instance;
		return &instance;
	}

	KYTY_CLASS_NO_COPY(Singleton);

protected:
	Singleton()  = default;
	~Singleton() = default;
};

} // namespace Common

#endif /* KYTY_COMMON_SINGLETON_H_ */
