#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <thread>

#include <omp.h>

double random01() { // thread safe rng
	thread_local std::mt19937 gen(std::random_device{}() + std::hash<std::thread::id>{}(std::this_thread::get_id()));
	thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
	return dist(gen);
}

void vector_shuffle(std::vector<int>& v) {  // safe randomness as long as multiple threads do not modify the same vector
	thread_local std::mt19937 rng(std::random_device{}() + std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::shuffle(v.begin(), v.end(), rng);
}

struct Graph{
	int num_vertices = 0;
	int num_edges = 0;
	
	std::vector<std::vector<int>> adj_list;

public:
	Graph() = default;
};

struct Subgraph{
	Graph G;
	std::vector<int> subgraph_mapping;
};

class Blockmodel {
public:
	Graph* G = NULL;
	int num_clusters = 0;
	std::vector<std::vector<int>> B; // cluster matrix
	std::vector<int> assignment;
	std::vector<int> num_vertices_per_block;

public:
	Blockmodel() = default;
	Blockmodel(Graph* G_, int num_clusters_): G(G_), num_clusters(num_clusters_){}
	~Blockmodel() {}
	void update_matrix();
	void move_vertex(int vertex, int new_cluster);
};

double compute_H(const Blockmodel& B) {  // description length (traditional blockmodel)
	if(B.G == NULL) {
		std::cout << "compute_H: Blockmodel with no graph\n";
		return 0.0;
	}
	
	double entropy = 0.0;
	for(int i=0;i<B.num_clusters; ++i) {
		for(int j=0; j < B.num_clusters; ++j) {
			entropy +=B.B[i][j] * log(B.B[i][j]/(B.num_vertices_per_block[i] * B.num_vertices_per_block[j]));
		}
	}
	
	entropy = B.G->num_edges - 0.5 * entropy;
	
	double x = (B.num_clusters * (B.num_clusters + 1)) / (2 * B.G->num_edges);
	double model_information = B.G->num_edges * ((1 + x) * log(1 + x) - x * log(x)) + B.G->num_vertices * log(B.num_clusters);
	
	double description_length = entropy + model_information;
	return description_length;
}

double compute_H_for_vertex_move(const Blockmodel& B, int vertex, int new_cluster) {  // description length (traditional blockmodel)
	if(B.G == NULL) {
		std::cout << "compute_H: Blockmodel with no graph\n";
		return 0.0;
	}
	
	std::vector<int> e_rs_minus(B.num_clusters, 0);
	std::vector<int> e_rs_plus(B.num_clusters, 0);
	
	for(int neighbour : B.G->adj_list[vertex]) {
		--e_rs_minus[B.assignment[neighbour]];
		e_rs_minus[B.assignment[neighbour]] -= (B.assignment[vertex] == B.assignment[neighbour]);
	
		++e_rs_plus[B.assignment[neighbour]];
		e_rs_plus[B.assignment[neighbour]] += (B.assignment[vertex] == B.assignment[neighbour]);
	}
	
	
	// should work for directed graphs; TODO: implement for undirected
	double entropy = 0.0;
	for(int i=0;i<B.num_clusters; ++i) {
		for(int j=0; j < B.num_clusters; ++j) {
			int old_cluster = B.assignment[vertex];
			int e_rs = B.B[i][j] + ((i == old_cluster) * e_rs_minus[j]) + ((i == new_cluster) * e_rs_plus[j]);
			int n_r = B.num_vertices_per_block[i] - (B.assignment[vertex] == i) + (new_cluster == i);
			int n_s = B.num_vertices_per_block[j] - (B.assignment[vertex] == j) + (new_cluster == j);
			
			entropy += e_rs * log(e_rs/(n_r * n_s));
		}
	}
	
	entropy = B.G->num_edges - 0.5 * entropy;
	
	double x = (B.num_clusters * (B.num_clusters + 1)) / (2 * B.G->num_edges);
	double model_information = B.G->num_edges * ((1 + x) * log(1 + x) - x * log(x)) + B.G->num_vertices * log(B.num_clusters);
	
	double description_length = entropy + model_information;
	return description_length;
}

void extract_subgraphs(const Blockmodel& B, std::vector<Subgraph>& subgraphs) {
	double threshold = 1.0;
	// subgraphs assumed to have size == B.num_clusters
	
	std::vector<std::unordered_map<int, int>> inverse_subgraph_mapping(subgraphs.size());
	
	for(size_t i = 0; i < B.assignment.size(); ++i) {
		if(random01() < threshold) {
			subgraphs[B.assignment[i]].subgraph_mapping.push_back(i);
			int index = subgraphs[B.assignment[i]].subgraph_mapping.size() - 1;
			inverse_subgraph_mapping[B.assignment[i]][i] = index;
		}
	}
	
	int k = 0;
	for(Subgraph& s : subgraphs) {
		s.G.num_vertices = s.subgraph_mapping.size();
		
		std::vector<std::vector<int>> adj_list(s.G.num_vertices);
		int num_edges = 0;
		for(int i=0; i<s.G.num_vertices; ++i) {
			for(int j : B.G->adj_list[s.subgraph_mapping[i]]) {
				if(inverse_subgraph_mapping[k].find(j) != inverse_subgraph_mapping[k].end()) {
					adj_list[i].push_back(inverse_subgraph_mapping[k][j]);
					++num_edges;
				}
			}
		}
	
		++k;
		
		s.G.num_edges = num_edges;
		std::swap(s.G.adj_list, adj_list);
	}
}

Blockmodel propose_split(Subgraph& subgraph, const Blockmodel& B) {
	Blockmodel result(&(subgraph.G), 2);
	
	int n = result.G->num_vertices;
	result.assignment.resize(n, 0);
	// random split
	for(int i = 0; i < n; ++i) {
		if(random01() < 0.5) {
			result.assignment[i] = 0;
		}
		else {
			result.assignment[i] = 1;	
		}
	}
	
	result.update_matrix();
	return result;
}

void apply_split(Blockmodel& B, const Blockmodel& split, const Subgraph& s) {
	int next_cluster_number = B.num_clusters;
	
	for(int i = 0; i < split.assignment.size(); ++i) {
		if(split.assignment[i] == 1) {
			B.assignment[s.subgraph_mapping[i]] = next_cluster_number;
		}
	}
	
	++B.num_clusters;
}

double compute_null_H(const Subgraph& s) {  // just the entropy, don't include the information necessary to describe the model via the matrix and the block assignments
	
	double result = s.G.num_edges * (1 - log((2 * s.G.num_edges) / (s.G.num_vertices * s.G.num_vertices)));

	return result;
}

void block_split(Blockmodel& B, int x, int c_prim) {
	double H = compute_H(B);
	
	struct best_split_with_deltaH {
		double deltaH;
		Blockmodel split;
		size_t subgraph_index;
		
	public:
		best_split_with_deltaH(): deltaH(std::numeric_limits<double>::max()), split(Blockmodel()){}
	};
	
	std::vector<best_split_with_deltaH> best_split((size_t)B.num_clusters);
		
	std::vector<Subgraph> subgraphs((size_t)B.num_clusters);
	
	extract_subgraphs(B, subgraphs);
	
	for(int i = 0; i < B.num_clusters; ++i){
		for(int j = 0; j < x; ++j) {
			Subgraph& Gc = subgraphs[i];
			Blockmodel B_prim = propose_split(Gc, B);
			double HB_prim = compute_H(B_prim);
			double deltaH = HB_prim - compute_null_H(Gc); // TODO
			if (deltaH < best_split[i].deltaH) {  // it should enter at least once through the if branch
				best_split[i].deltaH = deltaH;
				best_split[i].split = B_prim;
				best_split[i].subgraph_index = i;
			}
		}
	}
	
	std::sort(best_split.begin(), best_split.end(), [](const best_split_with_deltaH& a, const best_split_with_deltaH& b){return a.deltaH < b.deltaH;});	
	
	for(auto& item: best_split) {
		if(B.num_clusters >= c_prim) {
			break;
		}
		apply_split(B, item.split, subgraphs[item.subgraph_index]);
	}
	
	// update the matrix B.B
	B.update_matrix();
}

int propose_move(Blockmodel& B, int vertex, double H, double temperature) {
	int current_cluster = B.assignment[vertex];
	
	int best_cluster = current_cluster;
	double best_deltaH = std::numeric_limits<double>::max();
	
	for(int c=0; c < B.num_clusters; ++c) {
		if(c != current_cluster) {
			double H_prim = compute_H_for_vertex_move(B, vertex, c);
			double deltaH = H_prim - H;
			
			if(deltaH < best_deltaH) {
				best_deltaH = deltaH;
				best_cluster = c;
			}
		}
	}
	
	if(best_deltaH < 0.0) {
		return best_cluster;
	}
	else {
		double acceptance_probability = std::exp(-best_deltaH / temperature);
		if(random01() < acceptance_probability) {  // acceptance_probability in [0, 1]
			return best_cluster;
		}
		else {
			return -1;
		}
	}
	
}

void batched_parallel_mcmc(Blockmodel& B, double t, int n, int x, double temperature = 1.0) {
	double H = std::numeric_limits<double>::max();
	int i = 0;
	double deltaH = std::numeric_limits<double>::max();
	
	
	do {
		H = compute_H(B);
		
		std::vector<int> V(B.G->num_vertices);
		for(int i=0;i<V.size();++i){
			V[i] = i;
		}
		vector_shuffle(V);
		
		for(int batch =0; batch < n; ++batch) {
			int start = batch * B.G->num_vertices / n;
			int end = start + B.G->num_vertices / n;
			
			std::vector<int> M(B.G->num_vertices, -1);
			
			for(int i=start; i < end; ++i) {
				M[V[i]] = propose_move(B, V[i], H, temperature);
			}
			
			for(int v=0; v<M.size(); ++v) {
				if(M[v] > -1) {
					B.move_vertex(v, M[v]);  // completely updates the blockmodel
				}
			}
		}
		
		// B.update_matrix();  // is necessary?
		
		deltaH = compute_H(B) - H;
		++i;
	} while(deltaH > t * H && i < x);
}

Blockmodel stochastic_block_partitioning(Graph& G, int num_steps = 100) {
	Blockmodel B(&G, 1);
	
	B.assignment.resize(B.G->num_vertices, 0);
	B.update_matrix();
	
	double H = compute_H(B);
	double deltaH = std::numeric_limits<double>::max();
	
	int i=0;
	do {
		block_split(B, 10, 100);
		batched_parallel_mcmc(B, 1.0, 100, 100);
		
		deltaH = compute_H(B) - H; 
		
		++i;
	}while(i < num_steps);
	
	return B;
}

Graph read_graph(std::istream& is) {
	Graph G;
	
	int num_vertices = 0;
	is >> num_vertices;
	
}

int main(){
	
	return 0;
}

void Blockmodel::update_matrix() {
	std::vector<std::vector<int>> B_new(num_clusters, std::vector<int>(num_clusters, 0));
	std::vector<int> num_vertices_per_block_new(num_clusters, 0);
		
	for(int i=0;i<assignment.size();++i) {
		for(int j : G->adj_list[i]) {
			int r = assignment[i];
			int s = assignment[j];
			B_new[r][s] += ((r == s) * 1 + 1);
		}
		++num_vertices_per_block_new[assignment[i]];
	}
	
	std::swap(B, B_new);
	std::swap(num_vertices_per_block, num_vertices_per_block_new);
}

void Blockmodel::move_vertex(int vertex, int new_cluster) {
	int old_cluster = assignment[vertex];
	
	assignment[vertex] = new_cluster;
	
	// update the matrix B
	for(int neighbour : G->adj_list[vertex]) {
		int neighbour_cluster = assignment[neighbour];
		B[old_cluster][neighbour_cluster] -= (1 + (old_cluster == neighbour_cluster));
		B[new_cluster][neighbour_cluster] += (1 + (new_cluster == neighbour_cluster));
		
		// for the undirected graphs
		B[neighbour_cluster][old_cluster] -= (1 + (old_cluster == neighbour_cluster));
		B[neighbour_cluster][new_cluster] += (1 + (new_cluster == neighbour_cluster));
	}
	
	// update num_vertices_per_block
	--num_vertices_per_block[old_cluster];
	++num_vertices_per_block[new_cluster];
}














