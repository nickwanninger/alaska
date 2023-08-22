# using BenchmarkTools

struct Tree
	left::Union{Tree, Nothing}
	right::Union{Tree, Nothing}
end

function create_tree(depth::Int)
		if depth == 0
			return nothing
		end
		left = create_tree(depth - 1)
		right = create_tree(depth - 1)
		return Tree(left, right)
end


function num_nodes(tree)
	if isnothing(tree)
		return 0
	end
	return 1 + num_nodes(tree.right) + num_nodes(tree.left)
end

x = create_tree(4)
# @benchmark num_nodes(x)


