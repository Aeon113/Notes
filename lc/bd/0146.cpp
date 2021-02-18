struct lru_node {
	int value;
	int prev;
	int next;
};

class LRUCache {
public:
	LRUCache(int capacity) : _capacity(capacity),
				 _count(0),
				 _head(-1)	{
		memset(_lru_array, 0xff, sizeof(_lru_array));
	}
	
	int get(int key) {
		int val = _lru_array[key].value;
		if (val >= 0 && _head != key) {
			remove_node(key);
			insert_node(key);
		}

		return val;
	}
	
	void put(int key, int value) {
		if (_head != key) {
			if (_lru_array[key].value >= 0)
				remove_node(key);
			insert_node(key);
		}
		_lru_array[key].value = value;

		if (_count > _capacity) {
			int tail = _lru_array[_head].prev;
			remove_node(tail);
			_lru_array[tail].value = -1;
		}
	}

private:
	int _capacity;
	int _count;
	int _head;

	lru_node _lru_array[3010];

	void remove_node(int key) {
		// Must ensure key is not head.
		_lru_array[_lru_array[key].prev].next =
			_lru_array[key].next;
		_lru_array[_lru_array[key].next].prev =
			_lru_array[key].prev;
		--_count;
	}

	void insert_node(int key) {
		if (_count) {
			// Insert node to _head;
			_lru_array[_lru_array[_head].prev].next =
				key;
			_lru_array[key].prev = _lru_array[_head].prev;
			_lru_array[_head].prev = key;
			_lru_array[key].next =
				_head;
		} else {
			_lru_array[key].prev = key;
			_lru_array[key].next = key;
		}

		_head = key;
		++_count;
	}
};

/**
 * Your LRUCache object will be instantiated and called as such:
 * LRUCache* obj = new LRUCache(capacity);
 * int param_1 = obj->get(key);
 * obj->put(key,value);
 */
