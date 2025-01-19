#include <Arduino.h>

/*
 * MppJson.h FOR ESP32
 *
 *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *
 *
 *   simple json parser and properties
 */

#ifndef MPP_JSON_H_
#define MPP_JSON_H_

#define MAX_PROPERTIES 30

struct _KV {
	char* key;
	char* value;
	_KV* next;
};

// simple flat (k/v string pairs) json object
class MppJson {
public:
	MppJson();
	~MppJson();
	// returns nullptr if ok, error message if failure
	const char* loadFrom(const String jsonString);
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
	const _KV* getFirst() { return _properties; }
	int size(); // number of key/value pairs
private:
	_KV* _properties = NULL;
	_KV* _get(const char* key); // create if not found
	_KV* _find(const char* key);
};

// simple json array of Strings of MppJson objects
class MppJsonArray {
public:
	MppJsonArray();
	// returns nullptr if ok, error message if failure
	const char* loadFrom(const String jsonString);
	~MppJsonArray();
	bool hasNext();
	String next(); // get the next string in a json array, empty if none
private:
	unsigned int current = 0;
	String jsonString;
};

#endif /* MPP_JSON_H_ */
