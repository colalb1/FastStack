#include "seraph/stack.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
    using Clock = std::chrono::steady_clock;

    struct BenchmarkSample {
        std::string implementation;
        std::string operation;
        std::size_t iterations;
        int repeat_index;
        double total_ns;
        double nanoseconds_per_op;
        double ops_per_second;
    };

    struct BenchmarkAggregate {
        std::string implementation;
        std::string operation;
        std::size_t iterations;
        int repeats;
        double avg_nanoseconds_per_op;
        double avg_ops_per_second;
        double min_nanoseconds_per_op;
        double max_nanoseconds_per_op;
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

    template <typename Fn>
    std::vector<BenchmarkSample> run_samples(
            std::string_view impl_name,
            std::string_view operation,
            std::size_t iterations,
            int repeats,
            Fn&& fn
    ) {
        std::vector<BenchmarkSample> samples;
        samples.reserve(static_cast<std::size_t>(repeats));

        for (int repeat = 0; repeat < repeats; ++repeat) {
            const auto start = Clock::now();
            fn();
            const auto stop = Clock::now();
            const double measured_ns =
                    std::chrono::duration<double, std::nano>(stop - start).count();
            const double total_ns = std::max(1.0, measured_ns);
            const double ns_per_op = total_ns / static_cast<double>(iterations);
            const double ops_per_sec = 1e9 / ns_per_op;

            samples.push_back(BenchmarkSample{
                    .implementation = std::string(impl_name),
                    .operation = std::string(operation),
                    .iterations = iterations,
                    .repeat_index = repeat,
                    .total_ns = total_ns,
                    .nanoseconds_per_op = ns_per_op,
                    .ops_per_second = ops_per_sec,
            });
        }

        return samples;
    }

    template <typename Stack>
    std::vector<BenchmarkSample>
    bench_push_copy(std::string_view impl_name, std::size_t iterations, int repeats) {
        return run_samples(impl_name, "push_copy", iterations, repeats, [iterations]() {
            Stack stack;
            const int value = 42;
            for (std::size_t iii = 0; iii < iterations; ++iii) {
                stack.push(value);
            }
            g_sink += stack.size();
        });
    }

    template <typename Stack>
    std::vector<BenchmarkSample>
    bench_push_move(std::string_view impl_name, std::size_t iterations, int repeats) {
        return run_samples(impl_name, "push_move", iterations, repeats, [iterations]() {
            Stack stack;
            for (std::size_t iii = 0; iii < iterations; ++iii) {
                int value = static_cast<int>(iii);
                stack.push(std::move(value));
            }
            g_sink += stack.size();
        });
    }

    template <typename Stack>
    std::vector<BenchmarkSample>
    bench_emplace(std::string_view impl_name, std::size_t iterations, int repeats) {
        return run_samples(impl_name, "emplace", iterations, repeats, [iterations]() {
            Stack stack;
            for (std::size_t iii = 0; iii < iterations; ++iii) {
                stack.emplace(static_cast<int>(iii));
            }
            g_sink += stack.size();
        });
    }

    template <typename Stack>
    std::vector<BenchmarkSample>
    bench_pop(std::string_view impl_name, std::size_t iterations, int repeats) {
        return run_samples(impl_name, "pop", iterations, repeats, [iterations]() {
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
        });
    }

    template <typename Stack>
    std::vector<BenchmarkSample>
    bench_size(std::string_view impl_name, std::size_t iterations, int repeats) {
        return run_samples(impl_name, "size", iterations, repeats, [iterations]() {
            Stack stack;
            for (std::size_t iii = 0; iii < 1024; ++iii) {
                stack.emplace(static_cast<int>(iii));
            }

            std::uint64_t local_sum = 0;
            for (std::size_t iii = 0; iii < iterations; ++iii) {
                local_sum += stack.size();
            }
            g_sink += local_sum;
        });
    }

    template <typename Stack>
    std::vector<BenchmarkSample>
    bench_empty(std::string_view impl_name, std::size_t iterations, int repeats) {
        return run_samples(impl_name, "empty", iterations, repeats, [iterations]() {
            Stack stack;
            stack.emplace(1);
            std::uint64_t local_sum = 0;
            for (std::size_t iii = 0; iii < iterations; ++iii) {
                local_sum += static_cast<std::uint64_t>(stack.empty());
            }
            g_sink += local_sum;
        });
    }

    template <typename Stack>
    std::vector<BenchmarkSample>
    bench_top(std::string_view impl_name, std::size_t iterations, int repeats) {
        return run_samples(impl_name, "top", iterations, repeats, [iterations]() {
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
        });
    }

    std::vector<BenchmarkSample> bench_reserve_spinlock(std::size_t iterations, int repeats) {
        return run_samples("SpinlockStack", "reserve", iterations, repeats, [iterations]() {
            seraph::SpinlockStack<int> stack;
            for (std::size_t iii = 1; iii <= iterations; ++iii) {
                stack.reserve(iii);
            }
            g_sink += stack.size();
        });
    }

    std::vector<BenchmarkAggregate> build_aggregates(const std::vector<BenchmarkSample>& samples) {
        std::vector<BenchmarkAggregate> aggregates;
        std::map<std::pair<std::string, std::string>, std::vector<const BenchmarkSample*>> grouped;

        for (const auto& sample : samples) {
            grouped[{sample.implementation, sample.operation}].push_back(&sample);
        }

        for (const auto& [key, group] : grouped) {
            const auto& impl = key.first;
            const auto& op = key.second;

            double sum_ns_per_op = 0.0;
            double sum_ops_per_sec = 0.0;
            double min_ns_per_op = group.front()->nanoseconds_per_op;
            double max_ns_per_op = group.front()->nanoseconds_per_op;

            for (const auto* sample : group) {
                sum_ns_per_op += sample->nanoseconds_per_op;
                sum_ops_per_sec += sample->ops_per_second;
                min_ns_per_op = std::min(min_ns_per_op, sample->nanoseconds_per_op);
                max_ns_per_op = std::max(max_ns_per_op, sample->nanoseconds_per_op);
            }

            const double count = static_cast<double>(group.size());
            aggregates.push_back(BenchmarkAggregate{
                    .implementation = impl,
                    .operation = op,
                    .iterations = group.front()->iterations,
                    .repeats = static_cast<int>(group.size()),
                    .avg_nanoseconds_per_op = sum_ns_per_op / count,
                    .avg_ops_per_second = sum_ops_per_sec / count,
                    .min_nanoseconds_per_op = min_ns_per_op,
                    .max_nanoseconds_per_op = max_ns_per_op,
            });
        }

        return aggregates;
    }

    void write_results_csv(
            const std::vector<BenchmarkSample>& samples,
            const std::vector<BenchmarkAggregate>& aggregates,
            int repeats,
            const std::filesystem::path& output_path
    ) {
        std::ofstream out(output_path);
        out << "record_type,implementation,operation,iterations,repeats,repeat_index,total_ns,ns_"
               "per_op,ops_per_sec,min_ns_per_op,max_ns_per_op,avg_ns_per_op,avg_ops_per_sec\n";

        for (const auto& sample : samples) {
            out << "sample," << sample.implementation << "," << sample.operation << ","
                << sample.iterations << "," << repeats << "," << sample.repeat_index << ","
                << sample.total_ns << "," << sample.nanoseconds_per_op << ","
                << sample.ops_per_second << ",,,\n";
        }

        for (const auto& aggregate : aggregates) {
            out << "average," << aggregate.implementation << "," << aggregate.operation << ","
                << aggregate.iterations << "," << aggregate.repeats << ",,,,"
                << aggregate.min_nanoseconds_per_op << "," << aggregate.max_nanoseconds_per_op
                << "," << aggregate.avg_nanoseconds_per_op << "," << aggregate.avg_ops_per_second
                << "\n";
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

    std::string format_metric(double value) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(value >= 100.0 ? 1 : 2) << value;
        return ss.str();
    }

    void write_svg_grouped_bars(
            const std::vector<BenchmarkAggregate>& aggregates,
            const std::filesystem::path& output_path,
            bool use_ns_metric
    ) {
        std::vector<std::string> operations;
        for (const auto& result : aggregates) {
            if (std::find(operations.begin(), operations.end(), result.operation) ==
                operations.end()) {
                operations.push_back(result.operation);
            }
        }

        const std::vector<std::string> impls = {"SpinlockStack", "CASStack", "STLStack"};

        std::map<std::string, std::map<std::string, double>> metric_by_op_impl;
        double max_metric = 0.0;
        for (const auto& result : aggregates) {
            const double metric =
                    use_ns_metric ? result.avg_nanoseconds_per_op : result.avg_ops_per_second;
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
        const double bar_w = group_w / 5.0;

        std::ofstream out(output_path);
        out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\""
            << height << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
        out << "<rect x=\"0\" y=\"0\" width=\"" << width << "\" height=\"" << height
            << "\" fill=\"#ffffff\"/>\n";
        out << "<text x=\"" << width / 2
            << "\" y=\"40\" text-anchor=\"middle\" font-size=\"26\" font-family=\"Menlo, "
               "monospace\" fill=\"#111111\">Stack Performance Average: "
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
                << format_metric(value) << "</text>\n";
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
                out << "<text x=\"" << (x + bar_w / 2.0) << "\" y=\"" << (y - 6)
                    << "\" text-anchor=\"middle\" font-size=\"10\" font-family=\"Menlo, "
                       "monospace\" fill=\"#222222\">"
                    << format_metric(metric) << "</text>\n";
            }

            out << "<text x=\"" << center << "\" y=\"" << (height - margin_bottom + 20)
                << "\" text-anchor=\"middle\" font-size=\"12\" font-family=\"Menlo, "
                   "monospace\" fill=\"#222222\">"
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

    std::vector<BenchmarkSample> samples;
    samples.reserve(256);

    auto append_samples = [&samples](std::vector<BenchmarkSample> chunk) {
        samples.insert(
                samples.end(),
                std::make_move_iterator(chunk.begin()),
                std::make_move_iterator(chunk.end())
        );
    };

    using Spinlock = seraph::SpinlockStack<int>;
    using CAS = seraph::CASStack<int>;
    using STL = STLStackAdapter;

    append_samples(bench_push_copy<Spinlock>("SpinlockStack", iterations, repeats));
    append_samples(bench_push_copy<CAS>("CASStack", iterations, repeats));
    append_samples(bench_push_copy<STL>("STLStack", iterations, repeats));

    append_samples(bench_push_move<Spinlock>("SpinlockStack", iterations, repeats));
    append_samples(bench_push_move<CAS>("CASStack", iterations, repeats));
    append_samples(bench_push_move<STL>("STLStack", iterations, repeats));

    append_samples(bench_emplace<Spinlock>("SpinlockStack", iterations, repeats));
    append_samples(bench_emplace<CAS>("CASStack", iterations, repeats));
    append_samples(bench_emplace<STL>("STLStack", iterations, repeats));

    append_samples(bench_pop<Spinlock>("SpinlockStack", iterations, repeats));
    append_samples(bench_pop<CAS>("CASStack", iterations, repeats));
    append_samples(bench_pop<STL>("STLStack", iterations, repeats));

    append_samples(bench_size<Spinlock>("SpinlockStack", iterations, repeats));
    append_samples(bench_size<CAS>("CASStack", iterations, repeats));
    append_samples(bench_size<STL>("STLStack", iterations, repeats));

    append_samples(bench_empty<Spinlock>("SpinlockStack", iterations, repeats));
    append_samples(bench_empty<CAS>("CASStack", iterations, repeats));
    append_samples(bench_empty<STL>("STLStack", iterations, repeats));

    append_samples(bench_top<Spinlock>("SpinlockStack", iterations, repeats));
    append_samples(bench_top<STL>("STLStack", iterations, repeats));
    append_samples(bench_reserve_spinlock(iterations, repeats));

    const auto aggregates = build_aggregates(samples);

    const auto repo_root = find_repo_root();
    const auto output_dir = repo_root / "tests" / "perf_results";
    std::filesystem::create_directories(output_dir);

    const auto csv_path = output_dir / "stack_benchmark_results.csv";
    const auto ns_svg_path = output_dir / "stack_ns_per_op.svg";
    const auto ops_svg_path = output_dir / "stack_ops_per_sec.svg";

    write_results_csv(samples, aggregates, repeats, csv_path);
    write_svg_grouped_bars(aggregates, ns_svg_path, true);
    write_svg_grouped_bars(aggregates, ops_svg_path, false);

    std::cout << "Stack performance benchmark complete.\n";
    std::cout << "Results CSV: " << csv_path << "\n";
    std::cout << "Graph (ns/op, averaged): " << ns_svg_path << "\n";
    std::cout << "Graph (ops/sec, averaged): " << ops_svg_path << "\n";
    std::cout << "Sink: " << g_sink << "\n";

    return 0;
}
