#pragma once
#include "ExOverlapped.h"
#include "db_login_result.h"

struct DBOverlapped
{
	ExOverlapped ex_over;
	DBLoginResult result;
};
