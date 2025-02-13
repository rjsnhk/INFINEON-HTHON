import os
import subprocess
import sys
from enum import Enum
import filecmp
from dataclasses import dataclass
from typing import List


class SupportedProgrammingLanguages(Enum):
    py = 'py'
    cpp = 'cpp'
    java = 'java'
    cs = 'cs'


@dataclass
class LevelReport:
    level: str
    passed_tests: int
    total_tests: int
    level_lines: List[str]

    def __str__(self):
        st_line = [f"Level: {self.level}, Passed tests: {self.passed_tests}, Total tests: {self.total_tests}"]
        for level_line in self.level_lines:
            st_line.append(f'\t{level_line}')
        return '\n'.join(st_line)

    def __repr__(self):
        return self.__str__()


def read_config(config_file):
    config = {}
    with open(config_file, 'r') as file:
        for line in file:
            key, value = line.strip().split('=')
            config[key] = value
    return config


def print_help():
    help_message = """
Usage: solution_evaluator.py [config-file]

Arguments:
  config-file               (Optional) Path to the configuration file with paths to executables.
"""
    print(help_message)


def main(args):
    if len(args) > 2:
        print_help()
        return 1
    elif (len(args) == 2 and (args[1] == '-h' or args[1] == '--help')):
        print_help()
        return 0

    working_directory = os.getcwd()
    report_directory = os.getcwd()
    config_file = config = None
    for file in os.listdir(working_directory):
        if file.endswith('.config'):
            config_file = os.path.join(working_directory, file)
            break
    if config_file is not None:
        config = read_config(config_file)
    else:
        config = {}

    solution_directory = os.path.join(working_directory, 'solution')
    entry_point_file = find_entry_point_file(solution_directory)

    if entry_point_file is None:
        print("No entry point (Main) file found in the 'solution' directory.")
        print("Please go over the README or ask us questions to understand the expected structure.")
        return 1

    if not os.path.exists(os.path.join(working_directory, 'outputs')):
        os.makedirs(os.path.join(working_directory, 'outputs'))

    execute_main_file(entry_point_file, solution_directory, working_directory, config)

    report_stat = compare_outputs_with_golden(working_directory)
    report_stat.sort(key=lambda x: x.level)
    generate_report(report_stat, report_directory)


def find_entry_point_file(directory):
    main_files = {
        SupportedProgrammingLanguages.py: 'main.py',
        SupportedProgrammingLanguages.cpp: 'main.cpp',
        SupportedProgrammingLanguages.java: 'Main.java',
        SupportedProgrammingLanguages.cs: 'Program.cs'
    }

    extensions = [ext.value for ext in SupportedProgrammingLanguages]

    return next(
        (os.path.join(root, file) for extension in extensions for root, _, files in os.walk(directory)
         for file in files if file == main_files.get(SupportedProgrammingLanguages(extension))),
        None
    )


def execute_main_file(file_path, solution_directory, working_directory, config):
    extension = os.path.splitext(file_path)[1].lstrip('.')
    if extension not in SupportedProgrammingLanguages.__members__:
        raise ValueError(f"Unsupported file extension: {extension}")

    compile_command = None

    gpp_path = config.get('g++', 'g++')
    javac_path = config.get('javac', 'javac')
    java_path = config.get('java', 'java')
    python_path = config.get('python', 'python')
    dotnet_path = config.get('dotnet', 'dotnet')

    env = os.environ.copy()
    if gpp_path != 'g++':
        gpp_dir = os.path.dirname(gpp_path)
        env['PATH'] = f"{gpp_dir}{os.pathsep}{env['PATH']}"

    if extension == 'py':
        run_command_template = f'"{python_path}" "{file_path}"'
    elif extension == 'cpp':
        compile_command = f'{gpp_path} "{file_path}" -o "{solution_directory}/main" -I "{working_directory}/lib/cpp/pugixml-1.14/src/"'
        run_command_template = f'"{solution_directory}/main.exe"' if os.name == 'nt' else f'"{solution_directory}/main"'
    elif extension == 'java':
        compile_command = f'"{javac_path}" "{file_path}" -d "{solution_directory}"'
        run_command_template = f'"{java_path}" -cp "{solution_directory}" Main'
    elif extension == 'cs':
        run_command_template = f'"{dotnet_path}" run --project "{find_csproj_file(file_path)}"'
    else:
        raise ValueError(f"Unsupported file extension: {extension}")

    if compile_command:
        result = subprocess.run(compile_command, shell=True, capture_output=True, text=True, env=env)
        if result.stderr:
            raise ValueError(f"Compilation failed: {result.stderr}")
        if result.stdout:
            print(result.stdout)

    inputs_directory = os.path.join(working_directory, 'inputs')
    outputs_directory = os.path.join(working_directory, 'outputs')
    for root, _, files in os.walk(inputs_directory):
        for file in files:
            if file.endswith('.xml'):
                input_path = os.path.join(root, file)
                test_prefix = os.path.basename(input_path).split('_')[0]
                output_path = os.path.join(outputs_directory, os.path.relpath(root, inputs_directory),
                                           f'{test_prefix}_output.txt')
                if os.path.exists(output_path):
                    os.remove(output_path)
                os.makedirs(os.path.dirname(output_path), exist_ok=True)
                # TODO: Parallelize this for bigger test-cases?
                run_command(run_command_template, input_path, output_path, test_prefix, env)


def find_csproj_file(cs_file_path):
    directory = os.path.dirname(cs_file_path)
    for file in os.listdir(directory):
        if file.endswith('.csproj'):
            return os.path.join(directory, file)
    raise FileNotFoundError("No .csproj file found in the 'solution' directory adjacent to the Main.cs file.")


def run_command(command, input_path, output_path, test_prefix, env):
    console_queries_path = os.path.join(os.path.dirname(input_path), f'{test_prefix}_queries.txt')
    with open(console_queries_path, 'r') as cq_file:
        console_queries = cq_file.read()

    command = f'{command} "{input_path}"'
    result = subprocess.run(command, shell=True, input=console_queries, capture_output=True, text=True, env=env)

    with open(output_path, 'w') as f:
        if result.stdout:
            f.write(result.stdout)
        if result.stderr:
            raise ValueError(f"Execution failed: {result.stderr}")


def compare_outputs_with_golden(working_directory):
    outputs_directory = os.path.abspath(os.path.join(working_directory, 'outputs'))
    golden_directory = os.path.abspath(os.path.join(working_directory, 'golden'))
    report_stats = []  # List[LevelReport]

    for root, _, files in os.walk(outputs_directory):
        if not files:
            continue
        level_stat = LevelReport(level=os.path.basename(root), passed_tests=0, total_tests=0, level_lines=[])
        for file in files:
            if file.endswith('output.txt'):
                output_file = os.path.join(root, file)
                golden_file = os.path.join(golden_directory, os.path.relpath(root, outputs_directory), file)
                level_stat.total_tests += 1
                if not filecmp.cmp(output_file, golden_file, shallow=False):
                    level_stat.level_lines.append(f"Mismatch found in file: {output_file}")
                else:
                    level_stat.level_lines.append(f"File {output_file} matches the golden file.")
                    level_stat.passed_tests += 1
        report_stats.append(level_stat)

    return report_stats


def generate_report(report_stats, report_directory):
    overall_report = []
    for report in report_stats:
        overall_report.append(str(report))

    if not os.path.exists(report_directory):
        os.makedirs(report_directory)

    report_path = os.path.join(report_directory, 'evaluation_report.txt')
    with open(report_path, 'w') as report_file:
        report_file.write("\n".join(overall_report))

    print(f"Test report generated: {report_path}")


if __name__ == '__main__':
    main(sys.argv[1:])
