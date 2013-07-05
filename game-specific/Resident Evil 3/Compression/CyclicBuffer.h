#pragma once
#include <core.h>
#include <vector>

namespace Consolgames
{
class Stream;
}

namespace ResidentEvil
{

class CyclicBuffer
{
public:
	CyclicBuffer(int size);

	void write(const uint8* data, int size);
	void write(Consolgames::Stream* data, int size);
	void fill(uint8 value, int size);
	void reply(int backReference, int size);
	void readTail(Consolgames::Stream* data, int size, int backTo = 0) const;
	void readTail(uint8* data, int size, int backTo = 0) const;
	int size() const;

	uint8 at(int index) const;
	uint8 operator[](int index) const;

private:
	std::vector<uint8> m_data;
	int m_position;
};

}