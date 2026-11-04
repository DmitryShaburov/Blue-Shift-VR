
#include "VRRandom.h"

#include <random>

namespace
{
std::random_device random_device;

std::default_random_engine rng{random_device()};

std::default_random_engine rngbackup = rng;

std::uniform_real_distribution<float> floatdistribution(0.f, 1.f);
}

void VRRandomBackupSeed()
{
	rngbackup = rng;
}

void VRRandomRestoreSeed()
{
	rng = rngbackup;
}

float VRRandomFloat(float low, float high)
{
	return floatdistribution(rng) * (high - low) + low;
}

long VRRandomLong(long low, long high)
{
	return static_cast<long>(floatdistribution(rng) * (high + 1 - low)) + low;
}
