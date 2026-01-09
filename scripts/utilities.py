import sys

class TabbedStdout:
    def __init__(self, original, num_tabs : int):
        self.original = original
        self.at_line_start = True  # Track if we're at the start of a new line
        self.num_tabs = num_tabs

    def write(self, text):
        lines = text.splitlines(keepends=True)
        for line in lines:
            if self.at_line_start:
                self.original.write("".join(["\t" for _ in range(self.num_tabs)]))  # Add tab at the beginning of the line
            self.original.write(line)
            self.at_line_start = line.endswith("\n")  # Update line start flag

    def flush(self):
        self.original.flush()


def print_indented(f, num_tabs : int, *args, **kwargs):
    original_stdout = sys.stdout
    sys.stdout = TabbedStdout(original_stdout, num_tabs)
    
    temp = f(*args, **kwargs)  # All lines printed by f will now be tabbed
    
    sys.stdout = original_stdout
    
    return temp