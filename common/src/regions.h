#ifndef REGIONS_H
#define REGIONS_H

#include <map>
#include <string>

class Regions {

  public:

    static inline const std::string DEFAULT_REGION = "eu";

    // Region should not start with "l" to avoid confusion between priv and privl :)
    static inline const std::map<std::string, std::string> map = {
	{DEFAULT_REGION, "Europe"},
	{"sa", "South America"},
	{"as", "Asia"},
    };

    static std::string toString() {
	std::string s;
	for (const auto& [k, v] : Regions::map)
	    s.append(k).append(" (").append(v).append("), ");

	if (!s.empty())
		s = s.substr(0, s.size() - 2);
	return s;
    }

    static std::string toString(const std::string& key) {
	if (map.contains(key)) {
	    std::string s;
	    s.append(key).append(" (").append(map.at(key)).append(")");
	    return s;
	}
	return "unknown";
    }

    static const std::string regionFromPostfix(const std::string& in) {
	for (const auto& [k, v] : Regions::map)
	    if (in.ends_with(k))
		return k;

	return DEFAULT_REGION;
    }
};

#endif // REGIONS_H
