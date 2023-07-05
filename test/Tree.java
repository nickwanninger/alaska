class Tree {
	public Tree left = null;
	public Tree right = null;

	public static Tree create_tree(int depth) {
		if (depth == 0) {
			return null;
		}
		Tree n = new Tree();
		n.left = create_tree(depth - 1);
		n.right = create_tree(depth - 1);
		return n;
	}

	public static int num_nodes(Tree tree) {
		if (tree == null) {
			return 0;
		}
		return 1 + num_nodes(tree.right) + num_nodes(tree.left);
	}

	public static void main(String[] args) {
		long start = 0;
		long end = 0;
	
		start = System.nanoTime();
		Tree t = create_tree(27);
		end = System.nanoTime();
		System.out.printf("allocation took %fs\n", (end - start) / 1000.0 / 1000.0 / 1000.0);


		long trials = 20;
		for (int i = 0; i < trials; i++) {
			start = System.nanoTime();
			num_nodes(t);
			end = System.nanoTime();
			System.out.printf("%f\n", (end - start) / 1000.0 / 1000.0 / 1000.0);
		}
	}
}
