#include "Mpp32Json.h"
#include <stdlib.h>

/*
 * MppJson.cpp  FOR ESP32 !!
 *
 *
Author: MikeP  , adopted for ESP32 Ethernet by SergeyS
For devices managed by AutomationManager (AM)
 *
 */

MppJson::MppJson() {
}

const char *INVALID_JSON = "Invalid JSON";

const char* MppJson::loadFrom(String newProperties) {
	if (newProperties.length() < 2)
		return "Invalid length";
	else if (newProperties.charAt(0) != '{'
			|| newProperties.charAt(newProperties.length() - 1) != '}')
		return "Missing json delimiters";
	else {
		clear();
		unsigned i = 1;
		while (i < newProperties.length() - 1) {
			//			Serial.printf("now parsing '%s'...\n",newProperties.substring(i).c_str());
			int key1 = newProperties.indexOf('"', i);
			if (key1 == -1)
				return INVALID_JSON;
			++key1;
			int key2 = newProperties.indexOf('"', key1);
			if (key2 == -1)
				return INVALID_JSON;
			String key = newProperties.substring(key1, key2);
			int val1 = newProperties.indexOf(':', key2 + 1);
			if (val1 == -1)
				return INVALID_JSON;
			if (newProperties.substring(val1 + 1).startsWith("\"")) {
				// handle as string, remove quotes
				val1 = newProperties.indexOf('"', val1 + 1);
				if (val1 == -1)
					return INVALID_JSON;
				++val1;
				int val2 = newProperties.indexOf('"', val1);
				if (val2 == -1)
					return INVALID_JSON;
				String val = newProperties.substring(val1, val2);
				put(key.c_str(), val.c_str());
				i = val2 + 1;
				//				Serial.printf("found '%s'='%s' now at %d\n", key.c_str(),
				//						val.c_str(), i);
			} else if (newProperties.substring(val1 + 1).startsWith("null")) {
				put(key.c_str(), NULL);
				i = val1 + 5;
				//				Serial.printf("found '%s'=null now at %d\n", key.c_str(), i);
			} else {
				// handle as string
				++val1;
				int val2 = newProperties.indexOf(',', val1);
				if (val2 == -1)
					return INVALID_JSON;
				String val = newProperties.substring(val1, val2);
				put(key.c_str(), val.c_str());
				i = val2 + 1;
				//				Serial.printf("found '%s'='%s' now at %d\n", key.c_str(),
				//						val.c_str(), i);
			}
			if (newProperties.charAt(i) == ',')
				++i;
		}
		return nullptr;
	}
}

MppJson::~MppJson() {
	clear();
}

_KV* MppJson::_find(const char *key) {
	_KV *current = _properties;
	while (current != NULL) {
		if (strcmp(current->key, key) == 0)
			return current;
		current = current->next;
	}
	return NULL;
}

_KV* MppJson::_get(const char *key) {
	_KV *current = _find(key);
	if (current == NULL) {
		current = (_KV*) malloc(sizeof(_KV));
		current->key = (char*) malloc(strlen(key) + 1);
		strcpy(current->key, key);
		current->value = NULL;
		current->next = _properties;
		_properties = current;
	}
	return current;
}

void MppJson::remove(const char *key) {
	_KV *current = _find(key);
	if (current != NULL) {
		if (_properties == current) {
			_properties = current->next;
		} else {
			_KV *previous = _properties;
			while (previous->next != current)
				previous = previous->next;
			previous->next = current->next;
		}
		free(current->key);
		if (current->value)
			free(current->value);
		free(current);
	}
}

void MppJson::put(const char *key, const char *value) {
	_KV *current = _get(key);
	if (current->value != NULL)
		free(current->value);
	if (value != NULL) {
		current->value = (char*) malloc(strlen(value) + 1);
		strcpy(current->value, value);
	} else
		current->value = NULL;
}

const char* MppJson::get(const char *key) {
	_KV *current = _find(key);
	return current == NULL ? NULL : current->value;
}

bool MppJson::contains(const char *key) {
	return _find(key) != NULL;
}

bool MppJson::has(const char *key) {
	_KV *current = _find(key);
	return current != NULL && current->value != NULL
			&& strlen(current->value) > 0;
}

bool _toBool(const char *value) {
	if (value == NULL)
		return false;
	String test(value);
	test.toLowerCase();
	if (test == "true")
		return true;
	else if (test == "false")
		return false;
	else
		return test.toInt();
}

bool MppJson::is(const char *key) {
	return has(key) && _toBool(get(key));
}

int MppJson::getInt(const char *key) {
	const char *value = get(key);
	return value == NULL ? 0 : String(value).toInt();
}

unsigned MppJson::getUnsigned(const char *key) {
	return getInt(key);
}

float MppJson::getFloat(const char *key) {
	const char *value = get(key);
	return value == NULL ? 0 : String(value).toFloat();
}

void MppJson::clear() {
 // Serial.printf("Json clear! :%s key:%s\n",_properties,_properties->key);
	while (_properties != NULL)
		remove(_properties->key);
}

String MppJson::toString() {
	String result = "{";
	_KV *current = _properties;
	while (current != NULL) {
		if (result.length() > 1)
			result += ",";
		result += "\"" + String(current->key) + "\":";
		result +=
				current->value == NULL ?
						"null" : "\"" + String(current->value) + "\"";
		current = current->next;
	}
	result += "}";
	return result;
}

int MppJson::size() {
	int result = 0;
	_KV *current = _properties;
	while (current != NULL) {
		++result;
		current = current->next;
	}
	return result;
}

MppJsonArray::MppJsonArray() {
}

MppJsonArray::~MppJsonArray() {
}

const char* MppJsonArray::loadFrom(String newJsonString) {
	if (newJsonString.length() < 2)
		return "Invalid length";
	else if (newJsonString.charAt(0) != '['
			|| newJsonString.charAt(newJsonString.length() - 1) != ']')
		return "Missing json array delimiters";
	else {
		current = 0;
		jsonString = newJsonString;
		return nullptr;
	}
}

bool MppJsonArray::hasNext() {
	if (current >= jsonString.length())
		return false;
	int i = jsonString.indexOf('{',current);
	return !(i == -1 || jsonString.indexOf('}', i) == -1);
}

String MppJsonArray::next() {
	int i = jsonString.indexOf('{', current);
	String result = ""; // default
	current = jsonString.length(); // no next
	if (i != -1) {
		int j = jsonString.indexOf('}', i);
		if (j != -1) {
			current = j + 2;
			result = jsonString.substring(i, j + 1);
		}
	}
	return result;
}
