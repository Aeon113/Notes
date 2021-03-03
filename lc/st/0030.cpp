class MinStack {
public:
	/** initialize your data structure here. */
	MinStack() : size(0) {}
	
	void push(int x) {
		this->val[this->size] = x;
		if (!this->size)
			this->minval[this->size] = x;
		else
			this->minval[this->size] = std::min(this->minval[this->size - 1], x);
		++this->size;
	}
	
	void pop() {
		if (this->size)
			--this->size;
	}
	
	int top() {
		return this->val[this->size - 1];
	}
	
	int min() {
		return this->minval[this->size - 1];
	}

private:
	int val[20010];
	int minval[20010];
	int size;
};

/**
 * Your MinStack object will be instantiated and called as such:
 * MinStack* obj = new MinStack();
 * obj->push(x);
 * obj->pop();
 * int param_3 = obj->top();
 * int param_4 = obj->min();
 */
