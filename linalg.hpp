#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <algorithm>

struct Variable {
	std::optional<uint32_t> value;
};

struct Term {
	Variable* var;
	uint32_t mult;

	Term(Variable* var, uint32_t mult) : var(var), mult(mult) {}

	uint32_t get() {
		return var->value.value() * mult;
	}
};

struct Equation {
	Variable* lead;

	std::vector<Term> terms;
	uint32_t equated;

	void add_term(Variable* variable, uint32_t multiplier) {
		terms.emplace_back(variable, multiplier);
	}

	uint32_t try_solve() {
		auto iterator = terms.begin();
		while (iterator != terms.end()) {
			auto& term = *iterator;
			if (term.mult == 0 || (term.var->value.value_or(1) == 0))
				iterator = terms.erase(iterator);
			else
				++iterator;
		}

		if (terms.empty() && equated != 0)
			return 2; //not solvable

		if (terms.size() == 1) {
			auto& term = terms[0];
			auto& var = term.var;
			terms.clear();

			if (var->value.has_value()) {
				if (term.get() != equated)
					return 2; //not solvable
				return 3; //solved
			}
			else {
				if (equated % term.mult != 0) {
					printf("FUCK\n");
					return 2; //not solvable
				}
				var->value = equated / term.mult;
				return 3; //solved
			}
		}
		else {
			//remove non-free variables (terms already assigned)
			bool changed = false;
			auto iterator = terms.begin();

			while (iterator != terms.end()) {
				auto& term = *iterator;
				auto& val = term.var->value;
				if (val.has_value()) {
					changed = true;
					equated -= term.get();
					iterator = terms.erase(iterator);
				}
				else
					++iterator;
			}
			return changed ? 1 : 0;
		}
	}
};

struct LinearSystem {
	std::vector<Equation> equations;
	std::vector<std::unique_ptr<Variable>> variables;

	Variable* get_or_alloc_var(int index) {
		while (variables.size() <= index)
			variables.emplace_back(new Variable());
		return variables[index].get();
	}

	bool try_solve() {
		std::vector<Variable*> heads{};
		for (auto& equation : equations) {
			//add equation lead
			heads.push_back(equation.lead);
		}
		//assign random value to free variables
		for (auto& var : variables)
			if (std::find(heads.cbegin(), heads.cend(), var.get()) == heads.cend())
				var->value = __rdtsc() % static_cast<uint32_t>(-1);

		bool solving = true;
		while (solving) {
			solving = false;

			//make sure that all known variables are resolved
			bool any_changed = true;
			while (any_changed) {
				any_changed = false;
				auto iterator = equations.begin();
				while (iterator != equations.end()) {
					auto& equation = *iterator;
					switch (equation.try_solve()) {
					case 0: //unchanged equation
						++iterator;
						break;
					case 1: //changed equation
						++iterator;
						any_changed = true;
						break;
					case 2: //unsolvable equation
						return false;
					case 3: //solved equation
						iterator = equations.erase(iterator);
						any_changed = true;
						break;
					}
				}
			}
		}
		for (auto& variable : variables)
			if (!variable->value.has_value())
				variable->value = 1;
		return true;
	}
};