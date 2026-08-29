#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys

def collect_files(start_dir="."):
    """
    遍历 start_dir 下的所有文件，返回文件路径列表（相对路径）。
    """
    file_paths = []
    for root, dirs, files in os.walk(start_dir):
        for file in files:
            full_path = os.path.join(root, file)
            rel_path = os.path.relpath(full_path, start_dir)
            file_paths.append(rel_path)
    return file_paths

def read_file_content(file_path, base_dir="."):
    """
    尝试以 UTF-8 读取文件内容，若失败则返回 None。
    """
    full_path = os.path.join(base_dir, file_path)
    try:
        with open(full_path, "r", encoding="utf-8") as f:
            return f.read()
    except UnicodeDecodeError:
        # 可能是二进制文件，跳过
        return None
    except Exception as e:
        print(f"警告: 读取文件 {file_path} 时发生错误: {e}", file=sys.stderr)
        return None

def main():
    output_file = "all_files_content.txt"
    base_dir = "."  # 当前目录

    # 收集所有文件
    files = collect_files(base_dir)
    files.sort()  # 按路径排序

    with open(output_file, "w", encoding="utf-8") as out:
        for rel_path in files:
            # 跳过输出文件自身（避免自引用）
            if rel_path == output_file:
                continue

            content = read_file_content(rel_path, base_dir)
            if content is None:
                print(f"跳过二进制或无法解码的文件: {rel_path}", file=sys.stderr)
                continue

            # 写入文件头：路径分隔线
            out.write(f"===== 文件: {rel_path} =====\n")
            out.write(content)
            # 确保每个文件内容后换行，便于区分
            if not content.endswith("\n"):
                out.write("\n")
            out.write("\n")  # 额外空行分隔

    print(f"所有文件内容已写入: {output_file}")

if __name__ == "__main__":
    main()