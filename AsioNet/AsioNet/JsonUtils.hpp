#pragma once

#include <iostream>
#include <fstream>
#include <string>

#include "rapidjson/document.h"
#include "LogUtils.hpp"

namespace net_middleware
{
	class json_utils
	{
	public:
		static bool open_file(const char* filepath, rapidjson::Document& dom)
		{
			std::ifstream file(filepath);
			if (!file.is_open())
			{
				LOG("failed to open file: %s", filepath);
				return false;
			}

			std::string json_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			file.close();

			dom.Parse(json_content.c_str());
			if (dom.HasParseError())
			{
				LOG("invalid json format, error code: %d", dom.GetParseError());
				return false;
			}
			else
			{
				return true;
			}
		}
	};
}