using System;
using System.Diagnostics;
using System.Threading;

public class Tree {
	public Tree left = null;
	public Tree right = null;

	private static long nanoTime() {
		long nano = 10000L * Stopwatch.GetTimestamp();
		nano /= TimeSpan.TicksPerMillisecond;
		nano *= 100L;
		return nano;
	}

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

	public static void Main() {
		Tree t = create_tree(27);
		long trials = 20;
		for (int i = 0; i < trials; i++) {
			long start = nanoTime();
			num_nodes(t);
			long end = nanoTime();
			Console.WriteLine((end - start) / 1000.0 / 1000.0 / 1000.0);

		// System.out.printf("%f\n", (end - start) / 1000.0 / 1000.0 / 1000.0);
		}
	}
}
