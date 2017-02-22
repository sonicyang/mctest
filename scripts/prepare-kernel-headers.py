import os
import subprocess
import errno
import shutil
import re
import sys


kernel_path = ''
install_path = ''
patch_rules = []

arch = ''

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise

def patch_rule_append(find_pattern, replace):
    global patch_rules
    patch_rules.append((find_pattern, replace))

def file_patch(infile):
    with open(infile, 'r') as f:
        lines = f.readlines()

    with open(infile, 'w') as f:
        global patch_rules
        for line in lines:
            for rule in patch_rules:
                line = re.sub(rule[0], rule[1], line)
            f.write(line)


def header_check(header):
    global arch
    unrelated_header_types =['drivers', 'tools', 'scripts', 'security',
                            'sound', 'drm', 'kvm', 'xen', 'scsi', 'video']

    # skip unrelated architecture
    arch_path = 'arch/' + arch
    if 'arch/' in header and not arch_path in header:
        return False

    for h in unrelated_header_types:
        if h in header:
            return False

    return True

def file_patch_and_install(src_path):
    global kernel_path
    global install_path

    relative_path = src_path.split(kernel_path)[1]
    file = relative_path.rsplit('/')[-1]
    relative_dir = relative_path.split(file)[0]
    dest_dir = install_path + relative_dir

    if header_check(dest_dir) == False:
        return

    mkdir_p(dest_dir)
    shutil.copy2(src_path, dest_dir)
    dest_path = dest_dir + file

    file_patch(dest_path)


def main():
    """Main function."""
    argv = sys.argv

    assert len(argv) == 4, 'Invalid arguments'

    global kernel_path
    global install_path
    global arch

    kernel_path = argv[1]
    install_path = argv[2]
    arch = argv[3]

    # avoid the conflic with the 'new' operator in C++
    patch_rule_append('new', 'anew')

    # TODO: Add "extern "C"" to function declaration in string_64.h
    #       while we want to compile module with C++ code.
    if 'x86' in arch:
        patch_rule_append('void \*memset\(void \*s, int c, size_t n\)\;',
                        'extern \"C\" {\nvoid *memset(void *s, int c, size_t n);')
        patch_rule_append('int strcmp\(const char \*cs, const char \*ct\);',
                        'int strcmp(const char *cs, const char *ct);}')

    # wrap the declaration of extern function with extern "C"
    # e.g. extern void func(void); => extern "C" {void func(void);}
    def wrapped_with_externC(matched):
        func = matched.group(0).split('extern')[1]
        return 'extern \"C\" {' + func + '}'
    pattern = re.compile(r'^extern\s*[\w_][\w\d_]*[\s\*]*[\w_][\w\d_]*\(.*\);$')
    patch_rule_append(pattern, wrapped_with_externC)


    # avoid duplicated keyword definition
    # e.g. typedef _Bool                  bool;
    #     => #ifndef __cplusplus
    #        typedef _Bool                bool;
    #        #endif
    def wrapped_with_ifndef_cpluscplus_macro(matched):
        line = matched.group(0)
        return '#ifndef __cplusplus\n' + line + '\n#endif\n'
    pattern = re.compile(r'^\s*typedef.*\s*(false|true|bool);$')
    patch_rule_append(pattern, wrapped_with_ifndef_cpluscplus_macro)

    pattern = re.compile(r'^\s*(false|true|bool)\s*=.*$')
    patch_rule_append(pattern, wrapped_with_ifndef_cpluscplus_macro)

    # Use find command to find out all headers
    find_cmd = 'find -L ' + kernel_path + ' -name *.h'
    proc = subprocess.Popen(find_cmd, shell = True, stdout = subprocess.PIPE)

    lines = proc.stdout.readlines()

    for line in lines:
        if line == '':
            break

        # Remove the newline character
        src = line.replace('\n', "")
        file_patch_and_install(src)

if __name__ == '__main__':
    sys.exit(main())
