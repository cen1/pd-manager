#include "gtest/gtest.h"

#include "../../common/src/masl_protocol_2.h"
#include <cmath>
#include <stdint.h>

using namespace std;

void secondsPrettyPrint(uint32_t seconds) {
	uint32_t days = seconds / 60 / 60 / 24;
	uint32_t hours = (seconds / 60 / 60) % 24;
	uint32_t minutes = (seconds / 60) % 60;
	uint32_t secondz = seconds % 60;

	cout << days << "d " << hours << "h " << minutes << "m " << secondz << "s " << endl;
}

TEST(TestCases, BanDurationTest) {

	uint32_t UnixDay = 86400;
	uint32_t duration;

	//Ban points, number of games

	//Old formula for newbies < 100 games
	cout << "Testing new players" << endl << "===================" << endl;

	duration = MASL_PROTOCOL::CalcAutoBanDuration(0, 0);
	secondsPrettyPrint(duration);
	ASSERT_EQ(duration, UnixDay);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(2, 0);
	secondsPrettyPrint(duration);
	ASSERT_EQ(duration, UnixDay * 2);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(4, 0);
	secondsPrettyPrint(duration);
	ASSERT_EQ(duration, UnixDay * 3);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(6, 0);
	secondsPrettyPrint(duration);
	ASSERT_EQ(duration, UnixDay * 4);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(8, 0);
	secondsPrettyPrint(duration);
	ASSERT_EQ(duration, UnixDay * 5);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(10, 0);
	secondsPrettyPrint(duration);
	ASSERT_EQ(duration, UnixDay * 6);

	cout << endl;

	cout << "Testing 100 < games < 1000" << endl << "===================" << endl;

	duration = MASL_PROTOCOL::CalcAutoBanDuration(0, 500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(2, 500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(4, 500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(6, 500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(8, 500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(10, 500);
	secondsPrettyPrint(duration);

	cout << "Testing 1000 < games < 2000" << endl << "===================" << endl;

	duration = MASL_PROTOCOL::CalcAutoBanDuration(0, 1500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(2, 1500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(4, 1500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(6, 1500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(8, 1500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(10, 1500);
	secondsPrettyPrint(duration);

	cout << "Testing 2000 < games < 3000" << endl << "===================" << endl;

	duration = MASL_PROTOCOL::CalcAutoBanDuration(0, 2500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(2, 2500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(4, 2500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(6, 2500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(8, 2500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(10, 2500);
	secondsPrettyPrint(duration);

	cout << "Testing 3000 < games < 4000" << endl << "===================" << endl;

	duration = MASL_PROTOCOL::CalcAutoBanDuration(0, 3500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(2, 3500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(4, 3500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(6, 3500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(8, 3500);
	secondsPrettyPrint(duration);

	duration = MASL_PROTOCOL::CalcAutoBanDuration(10, 3500);
	secondsPrettyPrint(duration);

	cout << "Testing 4000 < games < 5000" << endl << "===================" << endl;

	uint32_t duration5k0 = MASL_PROTOCOL::CalcAutoBanDuration(0, 4500);
	secondsPrettyPrint(duration5k0);

	uint32_t duration5k2 = MASL_PROTOCOL::CalcAutoBanDuration(2, 4500);
	secondsPrettyPrint(duration5k2);

	uint32_t duration5k4 = MASL_PROTOCOL::CalcAutoBanDuration(4, 4500);
	secondsPrettyPrint(duration5k4);

	uint32_t duration5k6 = MASL_PROTOCOL::CalcAutoBanDuration(6, 4500);
	secondsPrettyPrint(duration5k6);

	uint32_t duration5k8 = MASL_PROTOCOL::CalcAutoBanDuration(8, 4500);
	secondsPrettyPrint(duration5k8);

	uint32_t duration5k10 = MASL_PROTOCOL::CalcAutoBanDuration(10, 4500);
	secondsPrettyPrint(duration5k10);

	cout << "Testing games > 5000" << endl << "===================" << endl;

	uint32_t duration5k0Plus = MASL_PROTOCOL::CalcAutoBanDuration(0, 1000000);
	secondsPrettyPrint(duration5k0Plus);
	ASSERT_EQ(duration5k0, duration5k0Plus);

	uint32_t duration5k2Plus = MASL_PROTOCOL::CalcAutoBanDuration(2, 1000000);
	secondsPrettyPrint(duration5k2Plus);
	ASSERT_EQ(duration5k2, duration5k2Plus);

	uint32_t duration5k4Plus = MASL_PROTOCOL::CalcAutoBanDuration(4, 1000000);
	secondsPrettyPrint(duration5k4Plus);
	ASSERT_EQ(duration5k4, duration5k4Plus);

	uint32_t duration5k6Plus = MASL_PROTOCOL::CalcAutoBanDuration(6, 1000000);
	secondsPrettyPrint(duration5k6Plus);
	ASSERT_EQ(duration5k6, duration5k6Plus);

	uint32_t duration5k8Plus = MASL_PROTOCOL::CalcAutoBanDuration(8, 1000000);
	secondsPrettyPrint(duration5k8Plus);
	ASSERT_EQ(duration5k8, duration5k8Plus);

	uint32_t duration5k10Plus = MASL_PROTOCOL::CalcAutoBanDuration(10, 1000000);
	secondsPrettyPrint(duration5k10Plus);
	ASSERT_EQ(duration5k10, duration5k10Plus);
}

int main(int argc, char** argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
	//cin.get();
}
