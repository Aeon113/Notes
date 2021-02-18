class CQueue {
public:
	CQueue() {}
	
	void appendTail(int value) {
		this->blah.push(value);
	}
	
	int deleteHead() {
		int result;
		if (this->reversed.empty()) {
			while (!this->blah.empty()) {
				this->reversed.push(this->blah.top());
				this->blah.pop();
			}

			if (this->reversed.empty())
				return -1;
		}

		result = this->reversed.top();
		this->reversed.pop();
		return result;
	}

private:
	std::stack<int> reversed;
	std::stack<int> blah;
};

/**
 * Your CQueue object will be instantiated and called as such:
 * CQueue* obj = new CQueue();
 * obj->appendTail(value);
 * int param_2 = obj->deleteHead();
 */
