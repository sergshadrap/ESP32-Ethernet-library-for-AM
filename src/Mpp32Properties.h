#include <Arduino.h>
#include "Mpp32Json.h"

/*
 * MppProperties.h FOR ESP32!!
 *
 */

#ifndef MPP_PROPERTIES_H_
#define MPP_PROPERTIES_H_

extern const char* P_PASSWORD;

class MppProperties {
public:
	MppProperties();
	~MppProperties();
	void begin();
	bool update(const String& newProperties);
	bool save();
	void clear();
	void put(const char* key, const char* value);
	void remove(const char* key);
	bool contains(const char* key); // if property is in the set
	bool has(const char* key); // if property has a value
	// returns the property as a string
	const char *get(const char* key);
	// returns true if the property is defined and is true
	bool is(const char* key);
	// returns the property as an int or the default 0 if missing or invalid
	int getInt(const char* key);
	unsigned getUnsigned(const char* key);
	float getFloat(const char* key);
	String toString();
	int size(); // number of k/v pairs
protected:
	MppJson properties;
};

#endif /* MPP_PROPERTIES_H_ */
