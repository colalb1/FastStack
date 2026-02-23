#include "seraph/stack.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using Clock = std::chrono::steady_clock;

    struct BenchmarkResult {
        std::string implementation;
        std::string operation;
        std::size_t iterations;
        double nanoseconds_per_op;
        double ops_per_second;
    };

    volatile std::uint64_t g_sink = 0;

    class STLStackAdapter {
      public:
        void push(const int& value) {
            data_.push(value);
        }

        void push(int&& value) {
            data_.push(std::move(value));
        }

        template <typename... Args> void emplace(Args&&... args) {
            data_.emplace(std::forward<Args>(args)...);
        }

        std::optional<int> pop() {
            if (data_.empty()) {
                return std::nullopt;
            }
            int value = std::move(data_.top());
            data_.pop();
            return value;
        }

        std::optional<int> top() const {
            if (data_.empty()) {
                return std::nullopt;
            }
            return data_.top();
        }

        bool empty() const noexcept {
            return data_.empty();
        }

        std::size_t size() const noexcept {
            return data_.size();
        }

      private:
        std::stack<int, std::vector<int>> data_;
    };

    std::filesystem::path find_repo_root() {
        std::filesystem::path current = std::filesystem::current_path();

        while (!current.empty()) {
            const auto marker = current / "include" / "seraph" / "stack.hpp";
            const auto cmake = current / "CMakeLists.txt";
            if (std::filesystem::exists(marker) && std::filesystem::exists(cmake)) {
                return current;
            }

            if (current == current.root_path()) {
                break;
            }
            current = current.parent_path();
        }

        throw std::runtime_error("Unable to find repository root from current working directory.");
    }

    template <typename Fn> double best_time_ns(Fn&& fn, int repeats) {
        double best = std::numeric_limits<double>::infinity();
        for (int iii = 0; iii < repeats; ++iii) {
            const auto start = Clock::now();
            fn();
            const auto stop = Clock::now();
            const auto elapsed_ns = std::chrono::duration<double, std::nano>(stop - start).count();
            best = std::min(best, elapsed_ns);
        }
        return best;
    }

    BenchmarkResult make_result(
            std::string implementation,
            std::string operation,
            std::size_t iterations,
            double total_ns
    ) {
        const double ns_per_op = total_ns / static_cast<double>(iterations);
        const double ops_per_sec = 1e9 / ns_per_op;
        return BenchmarkResult{
                .implementation = std::move(implementation),
                .operation = std::move(operation),
                .iterations = iterations,
                .nanoseconds_per_op = ns_per_op,
                .ops_per_second = ops_per_sec,
        };
    }

    template <typename Stack>
    BenchmarkResult
    bench_push_copy(std::string_view impl_name, std::size_t iterations, int repeats) {
        const double elapsed_ns = best_time_ns(
                [iterations]() {
                    Stack stack;
                    const int value = 42;
                    for (std::size_t iii = 0; iii < iterations; ++iii) {
                        stack.push(value);
                    }
                    g_sink += stack.size();
                },
                repeats
        );
        return make_result(std::string(impl_name), "push_copy", iterations, elapsed_ns);
    }

    template <typename Stack>
    BenchmarkResult
    bench_push_move(std::string_view impl_name, std::size_t iterations, int repeats) {
        const double elapsed_ns = best_time_ns(
                [iterations]() {
                    Stack stack;
                    for (std::size_t iii = 0; iii < iterations; ++iii) {
                        int value = static_cast<int>(iii);
                        stack.push(std::move(value));
                    }
                    g_sink += stack.size();
                },
                repeats
        );
        return make_result(std::string(impl_name), "push_move", iterations, elapsed_ns);
    }

    template <typename Stack>
    BenchmarkResult bench_emplace(std::string_view impl_name, std::size_t iterations, int repeats) {
        const double elapsed_ns = best_time_ns(
                [iterations]() {
                    Stack stack;
                    for (std::size_t iii = 0; iii < iterations; ++iii) {
                        stack.emplace(static_cast<int>(iii));
                    }
                    g_sink += stack.size();
                },
                repeats
        );
        return make_result(std::string(impl_name), "emplace", iterations, elapsed_ns);
    }

    template <typename Stack>
    BenchmarkResult bench_pop(std::string_view impl_name, std::size_t iterations, int repeats) {
        const double elapsed_ns = best_time_ns(
                [iterations]() {
                    Stack stack;
                    for (std::size_t iii = 0; iii < iterations; ++iii) {
                        stack.emplace(static_cast<int>(iii));
                    }

                    std::uint64_t local_sum = 0;
                    for (std::size_t iii = 0; iii < iterations; ++iii) {
                        auto value = stack.pop();
                        if (value.has_value()) {
                            local_sum += static_cast<std::uint64_t>(*value);
                        }
                    }
                    g_sink += local_sum;
                },
                repeats
        );
        return make_result(std::string(impl_name), "pop", iterations, elapsed_ns);
    }

    template <typename Stack>
    BenchmarkResult bench_size(std::string_view impl_name, std::size_t iterations, int repeats) {
        const double elapsed_ns = best_time_ns(
                [iterations]() {
                    Stack stack;
                    for (std::size_t iii = 0; iii < 1024; ++iii) {
                        stack.emplace(static_cast<int>(iii));
                    }

                    std::uint64_t local_sum = 0;
                    for (std::size_t iii = 0; iii < iterations; ++iii) {
                        local_sum += stack.size();
                    }
                    g_sink += local_sum;
                },
                repeats
        );
        return make_result(std::string(impl_name), "size", iterations, elapsed_ns);
    }

    template <typename Stack>
    BenchmarkResult bench_empty(std::string_view impl_name, std::size_t iterations, int repeats) {
        const double elapsed_ns = best_time_ns(
                [iterations]() {
                    Stack stack;
                    stack.emplace(1);
                    std::uint64_t local_sum = 0;
                    for (std::size_t iii = 0; iii < iterations; ++iii) {
                        local_sum += static_cast<std::uint64_t>(stack.empty());
                    }
                    g_sink += local_sum;
                },
                repeats
        );
        return make_result(std::string(impl_name), "empty", iterations, elapsed_ns);
    }

    template <typename Stack>
    BenchmarkResult bench_top(std::string_view impl_name, std::size_t iterations, int repeats) {
        const double elapsed_ns = best_time_ns(
                [iterations]() {
                    Stack stack;
                    stack.emplace(7);
                    std::uint64_t local_sum = 0;
                    for (std::size_t iii = 0; iii < iterations; ++iii) {
                        auto value = stack.top();
                        if (value.has_value()) {
                            local_sum += static_cast<std::uint64_t>(*value);
                        }
                    }
                    g_sink += local_sum;
                },
                repeats
        );
        return make_result(std::string(impl_name), "top", iterations, elapsed_ns);
    }

    BenchmarkResult bench_reserve_spinlock(std::size_t iterations, int repeats) {
        const double elapsed_ns = best_time_ns(
                [iterations]() {
                    seraph::SpinlockStack<int> stack;
                    for (std::size_t iii = 1; iii <= iterations; ++iii) {
                        stack.reserve(iii);
                    }
                    g_sink += stack.size();
                },
                repeats
        );
        return make_result("SpinlockStack", "reserve", iterations, elapsed_ns);
    }

    void write_results_csv(
            const std::vector<BenchmarkResult>& results,
            const std::filesystem::path& output_path
    ) {
        std::ofstream out(output_path);
        out << "implementation,operation,iterations,ns_per_op,ops_per_sec\n";
        for (const auto& result : results) {
            out << result.implementation << "," << result.operation << "," << result.iterations
                << "," << result.nanoseconds_per_op << "," << result.ops_per_second << "\n";
        }
    }

    std::string color_for_impl(std::string_view impl) {
        if (impl == "SpinlockStack") {
            return "#2a9d8f";
        }
        if (impl == "STLStack") {
            return "#264653";
        }
        return "#e76f51";
    }

    void write_svg_grouped_bars(
            const std::vector<BenchmarkResult>& results,
            const std::filesystem::path& output_path,
            bool use_ns_metric
    ) {
        std::vector<std::string> operations;
        for (const auto& result : results) {
            if (std::find(operations.begin(), operations.end(), result.operation) ==
                operations.end()) {
                operations.push_back(result.operation);
            }
        }

        const std::vector<std::string> impls = {"SpinlockStack", "CASStack", "STLStack"};

        std::map<std::string, std::map<std::string, double>> metric_by_op_impl;
        double max_metric = 0.0;
        for (const auto& result : results) {
            const double metric = use_ns_metric ? result.nanoseconds_per_op : result.ops_per_second;
            metric_by_op_impl[result.operation][result.implementation] = metric;
            max_metric = std::max(max_metric, metric);
        }

        const int width = 1280;
        const int height = 720;
        const int margin_left = 90;
        const int margin_right = 40;
        const int margin_top = 80;
        const int margin_bottom = 170;
        const double plot_w = static_cast<double>(width - margin_left - margin_right);
        const double plot_h = static_cast<double>(height - margin_top - margin_bottom);
        const double group_w = plot_w / static_cast<double>(operations.size());
        const double bar_w = group_w / 4.0;

        std::ofstream out(output_path);
        out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\""
            << height << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
        out << "<rect x=\"0\" y=\"0\" width=\"" << width << "\" height=\"" << height
            << "\" fill=\"#ffffff\"/>\n";
        out << "<text x=\"" << width / 2
            << "\" y=\"40\" text-anchor=\"middle\" font-size=\"26\" font-family=\"Menlo, "
               "monospace\" "
               "fill=\"#111111\">Stack Performance: "
            << (use_ns_metric ? "ns/op (lower is better)" : "ops/sec (higher is better)")
            << "</text>\n";

        for (int tick = 0; tick <= 5; ++tick) {
            const double ratio = static_cast<double>(tick) / 5.0;
            const double y = margin_top + plot_h - ratio * plot_h;
            const double value = ratio * max_metric;
            out << "<line x1=\"" << margin_left << "\" y1=\"" << y << "\" x2=\""
                << (width - margin_right) << "\" y2=\"" << y
                << "\" stroke=\"#e0e0e0\" stroke-width=\"1\"/>\n";
            out << "<text x=\"" << (margin_left - 10) << "\" y=\"" << (y + 4)
                << "\" text-anchor=\"end\" font-size=\"12\" font-family=\"Menlo, monospace\" "
                   "fill=\"#444444\">"
                << value << "</text>\n";
        }

        out << "<line x1=\"" << margin_left << "\" y1=\"" << margin_top << "\" x2=\"" << margin_left
            << "\" y2=\"" << (height - margin_bottom)
            << "\" stroke=\"#222222\" stroke-width=\"2\"/>\n";
        out << "<line x1=\"" << margin_left << "\" y1=\"" << (height - margin_bottom) << "\" x2=\""
            << (width - margin_right) << "\" y2=\"" << (height - margin_bottom)
            << "\" stroke=\"#222222\" stroke-width=\"2\"/>\n";

        for (std::size_t op_idx = 0; op_idx < operations.size(); ++op_idx) {
            const std::string& op = operations[op_idx];
            const double group_start = margin_left + static_cast<double>(op_idx) * group_w;
            const double center = group_start + group_w / 2.0;

            const auto op_it = metric_by_op_impl.find(op);
            if (op_it == metric_by_op_impl.end()) {
                continue;
            }

            std::vector<std::string> present_impls;
            for (const auto& impl : impls) {
                if (op_it->second.contains(impl)) {
                    present_impls.push_back(impl);
                }
            }

            for (std::size_t impl_idx = 0; impl_idx < present_impls.size(); ++impl_idx) {
                const std::string& impl = present_impls[impl_idx];
                const auto impl_it = op_it->second.find(impl);
                const double metric = impl_it->second;
                const double ratio = (max_metric > 0.0) ? (metric / max_metric) : 0.0;
                const double bar_h = ratio * plot_h;
                const double offset =
                        (static_cast<double>(impl_idx) - (present_impls.size() - 1) / 2.0) *
                        (bar_w + 8.0);
                const double x = center + offset - bar_w / 2.0;
                const double y = margin_top + plot_h - bar_h;

                out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << bar_w
                    << "\" height=\"" << bar_h << "\" fill=\"" << color_for_impl(impl) << "\"/>\n";
            }

            out << "<text x=\"" << center << "\" y=\"" << (height - margin_bottom + 20)
                << "\" text-anchor=\"middle\" font-size=\"12\" font-family=\"Menlo, monospace\" "
                   "fill=\"#222222\">"
                << op << "</text>\n";
        }

        const int legend_y = 60;
        int legend_x = 720;
        for (const auto& impl : impls) {
            out << "<rect x=\"" << legend_x << "\" y=\"" << (legend_y - 12)
                << "\" width=\"16\" height=\"16\" fill=\"" << color_for_impl(impl) << "\"/>\n";
            out << "<text x=\"" << (legend_x + 24) << "\" y=\"" << legend_y
                << "\" font-size=\"14\" font-family=\"Menlo, monospace\" fill=\"#222222\">" << impl
                << "</text>\n";
            legend_x += 170;
        }
        out << "</svg>\n";
    }

} // namespace

int main(int argc, char** argv) {
    bool quick = false;
    bool allow_debug = false;
    for (int iii = 1; iii < argc; ++iii) {
        const std::string arg(argv[iii]);
        if (arg == "--quick") {
            quick = true;
        }
        else if (arg == "--allow-debug") {
            allow_debug = true;
        }
    }

#ifndef NDEBUG
    if (!allow_debug) {
        std::cerr << "Error: benchmark must run in a Release build. Reconfigure with "
                     "`-DCMAKE_BUILD_TYPE=Release`.\n";
        std::cerr << "Use `--allow-debug` only for smoke validation.\n";
        return 2;
    }
#endif

    const std::size_t iterations = quick ? 20'000 : 300'000;
    const int repeats = quick ? 2 : 5;

    std::vector<BenchmarkResult> results;
    results.reserve(32);

    using Spinlock = seraph::SpinlockStack<int>;
    using CAS = seraph::CASStack<int>;
    using STL = STLStackAdapter;

    results.push_back(bench_push_copy<Spinlock>("SpinlockStack", iterations, repeats));
    results.push_back(bench_push_copy<CAS>("CASStack", iterations, repeats));
    results.push_back(bench_push_copy<STL>("STLStack", iterations, repeats));

    results.push_back(bench_push_move<Spinlock>("SpinlockStack", iterations, repeats));
    results.push_back(bench_push_move<CAS>("CASStack", iterations, repeats));
    results.push_back(bench_push_move<STL>("STLStack", iterations, repeats));

    results.push_back(bench_emplace<Spinlock>("SpinlockStack", iterations, repeats));
    results.push_back(bench_emplace<CAS>("CASStack", iterations, repeats));
    results.push_back(bench_emplace<STL>("STLStack", iterations, repeats));

    results.push_back(bench_pop<Spinlock>("SpinlockStack", iterations, repeats));
    results.push_back(bench_pop<CAS>("CASStack", iterations, repeats));
    results.push_back(bench_pop<STL>("STLStack", iterations, repeats));

    results.push_back(bench_size<Spinlock>("SpinlockStack", iterations, repeats));
    results.push_back(bench_size<CAS>("CASStack", iterations, repeats));
    results.push_back(bench_size<STL>("STLStack", iterations, repeats));

    results.push_back(bench_empty<Spinlock>("SpinlockStack", iterations, repeats));
    results.push_back(bench_empty<CAS>("CASStack", iterations, repeats));
    results.push_back(bench_empty<STL>("STLStack", iterations, repeats));

    results.push_back(bench_top<Spinlock>("SpinlockStack", iterations, repeats));
    results.push_back(bench_top<STL>("STLStack", iterations, repeats));
    results.push_back(bench_reserve_spinlock(iterations, repeats));

    const auto repo_root = find_repo_root();
    const auto output_dir = repo_root / "tests" / "perf_results";
    std::filesystem::create_directories(output_dir);

    const auto csv_path = output_dir / "stack_benchmark_results.csv";
    const auto ns_svg_path = output_dir / "stack_ns_per_op.svg";
    const auto ops_svg_path = output_dir / "stack_ops_per_sec.svg";

    write_results_csv(results, csv_path);
    write_svg_grouped_bars(results, ns_svg_path, true);
    write_svg_grouped_bars(results, ops_svg_path, false);

    std::cout << "Stack performance benchmark complete.\n";
    std::cout << "Results CSV: " << csv_path << "\n";
    std::cout << "Graph (ns/op): " << ns_svg_path << "\n";
    std::cout << "Graph (ops/sec): " << ops_svg_path << "\n";
    std::cout << "Sink: " << g_sink << "\n";

    return 0;
}
