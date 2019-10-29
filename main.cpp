#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <future>
#include <sstream>

#include "flint/flint.h"
#include "flint/fmpq_mat.h"
#include <set>
#include <random>
#include <utility>

#include "arith.h"
#include "linalg.hpp"

template <typename T>
constexpr T rol(T val, uint32_t N) {
	N %= sizeof(T) * CHAR_BIT;
	return (val << N) | (val >> (sizeof(T) * CHAR_BIT - N));
}

template <typename T>
constexpr T ror(T val, uint32_t N) {
	N %= sizeof(T) * CHAR_BIT;
	return (val << (sizeof(T) * CHAR_BIT - N)) | (val >> N);
}

uint32_t reverse(uint32_t n) {
	n = (n >> 1) & 0x55555555 | (n << 1) & 0xaaaaaaaa;
	n = (n >> 2) & 0x33333333 | (n << 2) & 0xcccccccc;
	n = (n >> 4) & 0x0f0f0f0f | (n << 4) & 0xf0f0f0f0;
	n = (n >> 8) & 0x00ff00ff | (n << 8) & 0xff00ff00;
	n = (n >> 16) & 0x0000ffff | (n << 16) & 0xffff0000;
	return n;
}

constexpr uint32_t test_cases[] = { 
	0, 1
};
constexpr size_t tc_size = sizeof(test_cases) / sizeof(uint32_t);

std::random_device device{};

#define sqr(x) ((x)*(x))

uint64_t sum(fmpz_mat_t mat)
{
	uint64_t s = 0;
	for (unsigned i = 0; i < mat->r; i++)
		for (unsigned j = 0; j < mat->c; j++)
			s += mat->rows[i][j];
	return s;
}

OpNode* gen_node(int depth)
{
	if (depth > 0)
	{
		switch (std::uniform_int_distribution<int>(0, 5)(device)) {
			//case 0: {
			//	const auto new_unop = new UnopNode();
			//	new_unop->type = static_cast<UnopNode::UnopType>(std::uniform_int_distribution<int>(0, static_cast<int>(UnopNode::UnopType::UNOP_MAX) - 1)(device));
			//	new_unop->right = gen_node(depth - 1);
			//	return new_unop;
			//}
			//case 1: {
			//	const auto new_binop = new BinopNode();
			//	new_binop->type = static_cast<BinopNode::BinopType>(std::uniform_int_distribution<int>(0, static_cast<int>(BinopNode::BinopType::BINOP_MAX) - 1)(device));
			//	new_binop->right = new VarNode(std::uniform_int_distribution<int>(0, 1)(device));
			//	new_binop->left = gen_node(depth - 1);
			//	return new_binop;
			//}
			//case 2: {
			//	const auto new_binop = new BinopNode();
			//	new_binop->type = static_cast<BinopNode::BinopType>(std::uniform_int_distribution<int>(0, static_cast<int>(BinopNode::BinopType::BINOP_MAX) - 1)(device));
			//	new_binop->right = new ConstantNode();
			//	new_binop->left = gen_node(depth - 1);
			//	return new_binop;
			//}
			default: {
				const auto new_binop = new BinopNode();
				new_binop->type = static_cast<BinopNode::BinopType>(std::uniform_int_distribution<int>(0, static_cast<int>(BinopNode::BinopType::BINOP_MAX) - 1)(device));
				new_binop->right = std::shared_ptr<OpNode>(gen_node(depth - 1));
				new_binop->left = std::shared_ptr<OpNode>(gen_node(depth - 1));
				return new_binop;
			}
		}
	}
	else
	{
		if (std::uniform_int_distribution<int>(0, 3)(device) > 0) {
			const auto new_unop = new UnopNode();
			new_unop->type = static_cast<UnopNode::UnopType>(std::uniform_int_distribution<int>(0, static_cast<int>(UnopNode::UnopType::UNOP_MAX) - 1)(device));
			new_unop->right = std::static_pointer_cast<OpNode>(std::make_shared<VarNode>(std::uniform_int_distribution<int>(0, 1)(device)));
			return new_unop;
		}
		return new VarNode(std::uniform_int_distribution<int>(0, 1)(device));
	}
}

void gen_nodes(int depth, unsigned count, std::vector<std::shared_ptr<OpNode>>& nodes)
{
	for (unsigned i = 0; i < count; i++)
		nodes.push_back(std::shared_ptr<OpNode>(gen_node(depth)));
}

void gen_from_test(std::function<uint32_t(uint32_t, uint32_t)> test, std::stringstream& out, int recurse = 0);

constexpr uint64_t m_mod = static_cast<uint32_t>(-1) + 1ULL;
bool solve_from_row_reduction(LinearSystem& linear_system, const nmod_mat_t A, const nmod_mat_t B)
{
	slong m = nmod_mat_nrows(A);
	slong n = nmod_mat_ncols(A);

	nmod_mat_t A2;
	nmod_mat_init(A2, m, n + 1, m_mod);

	slong i, j;
	for (i = 0; i < m; i++) {
		for (j = 0; j < n; j++)
			nmod_mat_entry(A2, i, j) = nmod_mat_entry(A, i, j);
		nmod_mat_entry(A2, i, n) = nmod_mat_entry(B, i, 0);
	}

	const int first_zero_row = nmod_mat_rref(A2);
	for (i = 0; i < first_zero_row; i++) {
		bool zeros = true;
		for (j = 0; j < n; j++)
			if (nmod_mat_entry(A2, i, j) != 0) {
				zeros = false;
				break;
			}
		if (zeros) {
			nmod_mat_clear(A2);
			return false;
		}
	}
	for (i = 0; i < first_zero_row; i++) {
		auto& equation = linear_system.equations.emplace_back();
		equation.equated = nmod_mat_entry(A2, i, n);
		int first_one = 0;
		while (first_one < n - 1 && nmod_mat_entry(A2, i, first_one) != 1)
			first_one++;
		if (nmod_mat_entry(A2, i, first_one) != 1) {
			nmod_mat_clear(A2);
			return false;
		}
		equation.lead = linear_system.get_or_alloc_var(first_one);

		for (int j = first_one; j < n; j++)
			equation.add_term(linear_system.get_or_alloc_var(j), nmod_mat_entry(A2, i, j));
	}
	nmod_mat_clear(A2);
	return linear_system.try_solve();
}

bool gen_variable(std::vector<std::shared_ptr<OpNode>>& funs, std::function<uint32_t(uint32_t, uint32_t)> test, std::stringstream& out, int recurse = 0) {
	nmod_mat_t A, B{};

	constexpr auto m_size = sqr(tc_size);
	const auto fun_size = funs.size();
	nmod_mat_init(A, m_size, fun_size, m_mod);
	nmod_mat_init(B, m_size, 1, m_mod);

	for (unsigned i = 0; i < tc_size; i++)
		for (unsigned j = 0; j < tc_size; j++)
			for (unsigned k = 0; k < fun_size; k++)
				nmod_mat_entry(A, i * tc_size + j, k) = funs[k]->compute(test_cases[i], test_cases[j]);

	for (unsigned i = 0; i < tc_size; i++)
		for (unsigned j = 0; j < tc_size; j++)
			nmod_mat_entry(B, i * tc_size + j, 0) = test(test_cases[i], test_cases[j]);

	LinearSystem linsys{};
	if (!solve_from_row_reduction(linsys, A, B))
		return false;

	nmod_mat_clear(A);
	nmod_mat_clear(B);

	std::stringstream out2;
	bool first = true;
	uint32_t res = 0;
	for (int j = 0; j < fun_size; j++) {
		const auto variable = linsys.variables[j]->value.value();
		if (variable == 0)
			continue;

		if (first)
			first = false;
		else
			out2 << " + ";

		res += static_cast<uint32_t>(variable) * funs[j]->compute(333, 2222);
		if (recurse > 0)
			gen_from_test([&](uint32_t a, uint32_t b) -> uint32_t {return static_cast<uint32_t>(variable)* funs[j]->compute(a, b); }, out2, recurse - 1);
		else
			out2 << "(" << static_cast<int32_t>(variable) <<" * " << funs[j]->to_string() << ")";
	}

	auto expected = test(333, 2222);
	if (res == expected) {
		out << out2.str();
		return true;
	}
	return false;
}

void gen_from_test(std::function<uint32_t(uint32_t, uint32_t)> test, std::stringstream& out, int recurse) {
	std::vector<std::shared_ptr<OpNode>> funs{};
	while (true) {
		funs.clear();
		gen_nodes(1,5, funs);
		if (gen_variable(funs, test, out))
			break;
	}
}

int main(int argc, char** argv)
{
	printf("OPERATION HEADASSIFIER\n");
	const auto test = [](uint32_t a, uint32_t b) -> uint32_t {
		return a + b;
	};

	while (true) {
		std::stringstream out;
		gen_from_test(test, out, 0);
		printf("%s\n", out.str().c_str());
	}
}
