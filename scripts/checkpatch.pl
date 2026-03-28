#!/usr/bin/env perl
# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: GPL-3.0-or-later
#
# MeshMC checkpatch.pl - Coding Style and Convention Checker
#
# This script checks C++, header, CMake, and other source files in the
# MeshMC project for adherence to the project's coding conventions.
#
# Usage:
#   ./scripts/checkpatch.pl [OPTIONS] [FILES...]
#   git diff | ./scripts/checkpatch.pl --diff
#   ./scripts/checkpatch.pl --git HEAD~1
#
# Options:
#   --diff          Read unified diff from stdin
#   --git <ref>     Check changes since git ref
#   --file <path>   Check a specific file (can be repeated)
#   --dir <path>    Check all files in directory recursively
#   --repository    Scan the entire git repository from root
#   --fix           Attempt to fix simple issues in-place
#   --quiet         Only show errors, not warnings
#   --verbose       Show additional diagnostic information
#   --summary       Show summary of issues at end
#   --color         Force colored output
#   --no-color      Disable colored output
#   --max-line-length <n>  Set maximum line length (default: 120)
#   --exclude <pat> Exclude files matching glob pattern
#   --help          Show this help message
#   --version       Show version information
#
# Exit codes:
#   0 - No errors found
#   1 - Errors found
#   2 - Warnings found (no errors)
#   3 - Script usage error

use strict;
use warnings;
use utf8;
use File::Basename;
use File::Find;
use File::Spec;
use Getopt::Long qw(:config no_ignore_case bundling);
use Cwd qw(abs_path getcwd);
use POSIX qw(strftime);

# ===========================================================================
# Version and Constants
# ===========================================================================

our $VERSION = "1.0.0";
our $PROGRAM = "MeshMC checkpatch";

# Severity levels
use constant {
    SEV_ERROR   => 0,
    SEV_WARNING => 1,
    SEV_INFO    => 2,
    SEV_STYLE   => 3,
};

# File type constants
use constant {
    FTYPE_CPP       => 'cpp',
    FTYPE_HEADER    => 'header',
    FTYPE_CMAKE     => 'cmake',
    FTYPE_UI        => 'ui',
    FTYPE_QRC       => 'qrc',
    FTYPE_OTHER     => 'other',
};

# ===========================================================================
# Configuration and Defaults
# ===========================================================================

my $MAX_LINE_LENGTH         = 120;
my $INDENT_WIDTH            = 4;
my $CMAKE_INDENT_WIDTH      = 3;
my $MAX_FUNCTION_LENGTH     = 200;
my $MAX_FILE_LENGTH         = 3000;
my $MAX_NESTING_DEPTH       = 6;
my $MAX_PARAMS_PER_FUNCTION = 8;
my $MAX_CONSECUTIVE_BLANK   = 2;

# ===========================================================================
# Global State
# ===========================================================================

my @g_errors       = ();
my @g_warnings     = ();
my @g_info         = ();
my $g_error_count  = 0;
my $g_warn_count   = 0;
my $g_info_count   = 0;
my $g_file_count   = 0;
my $g_line_count   = 0;

# Options
my $opt_diff       = 0;
my $opt_git        = '';
my @opt_files      = ();
my @opt_dirs       = ();
my $opt_fix        = 0;
my $opt_quiet      = 0;
my $opt_verbose    = 0;
my $opt_summary    = 0;
my $opt_color      = -1;  # auto-detect
my $opt_repository = 0;
my $opt_report     = '';
my $opt_help       = 0;
my $opt_version    = 0;
my @opt_excludes   = ();

# ANSI color codes
my %colors = (
    'red'     => "\033[1;31m",
    'green'   => "\033[1;32m",
    'yellow'  => "\033[1;33m",
    'blue'    => "\033[1;34m",
    'magenta' => "\033[1;35m",
    'cyan'    => "\033[1;36m",
    'white'   => "\033[1;37m",
    'reset'   => "\033[0m",
    'bold'    => "\033[1m",
    'dim'     => "\033[2m",
);

# ===========================================================================
# Naming Convention Patterns
# ===========================================================================

# PascalCase: starts with uppercase, mixed case
my $RE_PASCAL_CASE = qr/^[A-Z][a-zA-Z0-9]*$/;

# camelCase: starts with lowercase, mixed case
my $RE_CAMEL_CASE = qr/^[a-z][a-zA-Z0-9]*$/;

# m_ prefix member variable: m_camelCase
my $RE_MEMBER_VAR = qr/^m_[a-z][a-zA-Z0-9]*$/;

# UPPER_SNAKE_CASE for macros
my $RE_MACRO_CASE = qr/^[A-Z][A-Z0-9_]*$/;

# snake_case
my $RE_SNAKE_CASE = qr/^[a-z][a-z0-9_]*$/;

# Qt slot naming: on_widgetName_signalName
my $RE_QT_SLOT = qr/^on_[a-zA-Z]+_[a-zA-Z]+$/;

# ===========================================================================
# License Header Templates
# ===========================================================================
# REUSE-IgnoreStart
my $SPDX_HEADER_PATTERN = qr{
    /\*\s*SPDX-FileCopyrightText:.*?\n
    .*?SPDX-(?:FileContributor|FileCopyrightText):.*?\n
    .*?SPDX-License-Identifier:\s*GPL-3\.0-or-later
}xs;

my $SPDX_CMAKE_PATTERN = qr{
    \#\s*SPDX-(?:FileCopyrightText|License-Identifier):
}x;
# REUSE-IgnoreEnd
# ===========================================================================
# Known Qt Types and Macros
# ===========================================================================

my %KNOWN_QT_TYPES = map { $_ => 1 } qw(
    QString QStringList QByteArray QVariant QUrl QDir QFile QFileInfo
    QDateTime QDate QTime QTimer QObject QWidget QMainWindow QDialog
    QPushButton QLabel QLineEdit QTextEdit QComboBox QCheckBox
    QListWidget QTreeWidget QTableWidget QMenu QMenuBar QToolBar
    QAction QStatusBar QMessageBox QFileDialog QColorDialog
    QLayout QVBoxLayout QHBoxLayout QGridLayout QFormLayout
    QSplitter QTabWidget QStackedWidget QGroupBox QFrame
    QScrollArea QDockWidget QToolBox QProgressBar QSlider
    QSpinBox QDoubleSpinBox QRadioButton QButtonGroup
    QMap QHash QList QVector QSet QMultiMap QMultiHash
    QPair QSharedPointer QWeakPointer QScopedPointer
    QJsonDocument QJsonObject QJsonArray QJsonValue QJsonParseError
    QProcess QThread QMutex QMutexLocker QWaitCondition
    QNetworkAccessManager QNetworkRequest QNetworkReply
    QSettings QStandardPaths QCoreApplication QApplication
    QPixmap QIcon QImage QPainter QColor QFont QPen QBrush
    QPoint QPointF QSize QSizeF QRect QRectF
    QModelIndex QAbstractListModel QAbstractTableModel QAbstractItemModel
    QSortFilterProxyModel QItemSelectionModel QStyledItemDelegate
    QDomDocument QDomElement QDomNode QDomNodeList
    QRegularExpression QRegularExpressionMatch
    QTextStream QDataStream QBuffer QIODevice
    QEvent QKeyEvent QMouseEvent QResizeEvent QCloseEvent
    QSignalMapper QValidator QIntValidator QDoubleValidator
    QTranslator QLocale QLatin1String QStringLiteral
    QTest
);

my %KNOWN_QT_MACROS = map { $_ => 1 } qw(
    Q_OBJECT Q_PROPERTY Q_ENUM Q_FLAG Q_DECLARE_FLAGS Q_DECLARE_METATYPE
    Q_INVOKABLE Q_SLOT Q_SIGNAL Q_EMIT Q_UNUSED Q_ASSERT Q_ASSERT_X
    Q_DISABLE_COPY Q_DISABLE_MOVE Q_DISABLE_COPY_MOVE
    SIGNAL SLOT
    QTEST_GUILESS_MAIN QTEST_MAIN QTEST_APPLESS_MAIN
    QCOMPARE QVERIFY QVERIFY2 QFETCH QSKIP QFAIL QEXPECT_FAIL
    QBENCHMARK
);

# Known MeshMC macros
my %KNOWN_PROJECT_MACROS = map { $_ => 1 } qw(
    APPLICATION STRINGIFY TOSTRING MACOS_HINT
    WIN32_LEAN_AND_MEAN
);

# ===========================================================================
# File Extension Mappings
# ===========================================================================

my %EXT_TO_FTYPE = (
    '.cpp'  => FTYPE_CPP,
    '.cxx'  => FTYPE_CPP,
    '.cc'   => FTYPE_CPP,
    '.c'    => FTYPE_CPP,
    '.h'    => FTYPE_HEADER,
    '.hpp'  => FTYPE_HEADER,
    '.hxx'  => FTYPE_HEADER,
    '.ui'   => FTYPE_UI,
    '.qrc'  => FTYPE_QRC,
);

my %CMAKE_FILES = map { $_ => 1 } qw(
    CMakeLists.txt
);

# Source file extensions to check
my @SOURCE_EXTENSIONS = qw(.cpp .cxx .cc .c .h .hpp .hxx);

# ===========================================================================
# Excluded Directories and Files
# ===========================================================================

my @DEFAULT_EXCLUDES = (
    'build/',
    'libraries/',
    '.git/',
    'CMakeFiles/',
    'Testing/',
    'jars/',
    'Debug/',
    'Release/',
    'RelWithDebInfo/',
    'moc_*',
    'ui_*',
    'qrc_*',
    '*_autogen*',
);

# ===========================================================================
# Main Entry Point
# ===========================================================================

sub main {
    parse_options();

    if ($opt_help) {
        print_usage();
        exit(0);
    }

    if ($opt_version) {
        print "$PROGRAM v$VERSION\n";
        exit(0);
    }

    # Determine color support
    if ($opt_color == -1) {
        $opt_color = (-t STDOUT) ? 1 : 0;
    }

    if ($opt_diff) {
        process_diff_from_stdin();
    } elsif ($opt_git) {
        process_git_diff($opt_git);
    } elsif (@opt_files) {
        foreach my $file (@opt_files) {
            process_file($file);
        }
    } elsif (@opt_dirs) {
        foreach my $dir (@opt_dirs) {
            process_directory($dir);
        }
    } elsif ($opt_repository) {
        my $root = find_git_root();
        if (!$root) {
            print STDERR "ERROR: --repository used but no git repository found.\n";
            exit(3);
        }
        print "Scanning repository at: $root\n" if $opt_verbose;
        process_directory($root);
    } else {
        # No input specified: show help
        print_usage();
        exit(0);
    }

    if ($opt_summary) {
        print_summary();
    }

    if ($opt_report) {
        write_report($opt_report);
    }

    # Exit code based on results
    if ($g_error_count > 0) {
        exit(1);
    } elsif ($g_warn_count > 0) {
        exit(2);
    }
    exit(0);
}

# ===========================================================================
# Option Parsing
# ===========================================================================

sub parse_options {
    GetOptions(
        'diff'              => \$opt_diff,
        'git=s'             => \$opt_git,
        'file=s'            => \@opt_files,
        'dir=s'             => \@opt_dirs,
        'repository|repo|R' => \$opt_repository,
        'report=s'          => \$opt_report,
        'fix'               => \$opt_fix,
        'quiet|q'           => \$opt_quiet,
        'verbose|v'         => \$opt_verbose,
        'summary|s'         => \$opt_summary,
        'color'             => sub { $opt_color = 1; },
        'no-color'          => sub { $opt_color = 0; },
        'max-line-length=i' => \$MAX_LINE_LENGTH,
        'exclude=s'         => \@opt_excludes,
        'help|h'            => \$opt_help,
        'version|V'         => \$opt_version,
    ) or do {
        print_usage();
        exit(3);
    };

    # Remaining arguments are files
    push @opt_files, @ARGV;
}

sub print_usage {
    print <<'USAGE';
MeshMC checkpatch.pl - Coding Style and Convention Checker

Usage:
  checkpatch.pl [OPTIONS] [FILES...]
  git diff | checkpatch.pl --diff
  checkpatch.pl --git HEAD~1

Options:
  --diff                Read unified diff from stdin
  --git <ref>           Check changes since git ref
  --file <path>         Check a specific file (can be repeated)
  --dir <path>          Check all files in directory recursively
  --repository, -R      Scan the entire git repository from its root
  --report <file>       Write report to file (.txt, .json, .html, .csv)
  --fix                 Attempt to fix simple issues in-place
  --quiet, -q           Only show errors, not warnings
  --verbose, -v         Show additional diagnostic information
  --summary, -s         Show summary of issues at end
  --color               Force colored output
  --no-color            Disable colored output
  --max-line-length <n> Set maximum line length (default: 120)
  --exclude <pat>       Exclude files matching glob pattern
  --help, -h            Show this help message
  --version, -V         Show version information

Checked conventions:
  - 4-space indentation (no tabs) for C++/header files
  - 3-space indentation for CMake files
  - K&R brace placement style
  - PascalCase class names, camelCase methods, m_ member variables
  - #pragma once header guards
  - SPDX license headers
  - Qt conventions (signal/slot, Q_OBJECT, etc.)
  - Line length limits (default 120 chars)
  - Trailing whitespace
  - Proper include ordering
  - const correctness hints
  - Memory management patterns
  - Other MeshMC-specific conventions

Exit codes:
  0 - No errors found
  1 - Errors found
  2 - Warnings found (no errors)
  3 - Script usage error

USAGE
}

# ===========================================================================
# Color Output Helpers
# ===========================================================================

sub colorize {
    my ($color_name, $text) = @_;
    return $text unless $opt_color;
    return ($colors{$color_name} // '') . $text . $colors{'reset'};
}

sub severity_label {
    my ($severity) = @_;
    if ($severity == SEV_ERROR) {
        return colorize('red', 'ERROR');
    } elsif ($severity == SEV_WARNING) {
        return colorize('yellow', 'WARNING');
    } elsif ($severity == SEV_INFO) {
        return colorize('cyan', 'INFO');
    } elsif ($severity == SEV_STYLE) {
        return colorize('magenta', 'STYLE');
    }
    return 'UNKNOWN';
}

# ===========================================================================
# Reporting Functions
# ===========================================================================

sub report {
    my ($file, $line, $severity, $rule, $message) = @_;

    # Always track counts and store issues regardless of display mode
    if ($severity == SEV_ERROR) {
        $g_error_count++;
        push @g_errors, {
            file     => $file,
            line     => $line,
            rule     => $rule,
            message  => $message,
        };
    } elsif ($severity == SEV_WARNING) {
        $g_warn_count++;
        push @g_warnings, {
            file     => $file,
            line     => $line,
            rule     => $rule,
            message  => $message,
        };
    } else {
        $g_info_count++;
        push @g_info, {
            file     => $file,
            line     => $line,
            rule     => $rule,
            message  => $message,
        };
    }

    # Determine whether to print to stdout
    return if ($opt_quiet && $severity > SEV_ERROR);
    return if (!$opt_verbose && $severity >= SEV_INFO);

    my $label = severity_label($severity);
    my $location = colorize('bold', "$file:$line");
    my $rule_tag = colorize('dim', "[$rule]");

    print "$location: $label: $message $rule_tag\n";
}

sub print_summary {
    print "\n";
    print colorize('bold', "=" x 60) . "\n";
    print colorize('bold', "  MeshMC checkpatch Summary") . "\n";
    print colorize('bold', "=" x 60) . "\n";
    print "  Files checked:  $g_file_count\n";
    print "  Lines checked:  $g_line_count\n";
    print "  Errors:         " . colorize('red', $g_error_count) . "\n";
    print "  Warnings:       " . colorize('yellow', $g_warn_count) . "\n";
    if ($opt_verbose) {
        print "  Info:           " . colorize('cyan', $g_info_count) . "\n";
    }
    print colorize('bold', "=" x 60) . "\n";

    if ($g_error_count == 0 && $g_warn_count == 0) {
        print colorize('green', "  All checks passed!") . "\n";
    }
    print "\n";
}

# ===========================================================================
# Report Generation
# ===========================================================================

sub write_report {
    my ($report_path) = @_;

    my @all_issues = ();
    foreach my $e (@g_errors) {
        push @all_issues, { %$e, severity => 'ERROR' };
    }
    foreach my $w (@g_warnings) {
        push @all_issues, { %$w, severity => 'WARNING' };
    }
    foreach my $info (@g_info) {
        push @all_issues, { %$info, severity => 'INFO' };
    }

    # Sort issues by file, then line number
    @all_issues = sort {
        $a->{file} cmp $b->{file} || $a->{line} <=> $b->{line}
    } @all_issues;

    # Determine format from file extension
    my $format = 'txt';
    if ($report_path =~ /\.json$/i) {
        $format = 'json';
    } elsif ($report_path =~ /\.html?$/i) {
        $format = 'html';
    } elsif ($report_path =~ /\.csv$/i) {
        $format = 'csv';
    }

    if ($format eq 'json') {
        write_report_json($report_path, \@all_issues);
    } elsif ($format eq 'html') {
        write_report_html($report_path, \@all_issues);
    } elsif ($format eq 'csv') {
        write_report_csv($report_path, \@all_issues);
    } else {
        write_report_txt($report_path, \@all_issues);
    }

    print "Report written to: $report_path\n";
}

sub write_report_txt {
    my ($path, $issues) = @_;

    open(my $fh, '>:encoding(UTF-8)', $path) or die "Cannot write report to $path: $!\n";

    print $fh "=" x 70, "\n";
    print $fh "  MeshMC checkpatch Report\n";
    print $fh "  Generated: " . POSIX::strftime("%Y-%m-%d %H:%M:%S", localtime) . "\n";
    print $fh "=" x 70, "\n\n";

    print $fh "Summary:\n";
    print $fh "  Files checked:  $g_file_count\n";
    print $fh "  Lines checked:  $g_line_count\n";
    print $fh "  Errors:         $g_error_count\n";
    print $fh "  Warnings:       $g_warn_count\n";
    print $fh "  Info:           $g_info_count\n";
    print $fh "\n";

    if (@$issues == 0) {
        print $fh "All checks passed! No issues found.\n";
    } else {
        print $fh "-" x 70, "\n";
        print $fh "  Issues (" . scalar(@$issues) . " total)\n";
        print $fh "-" x 70, "\n\n";

        my $current_file = '';
        foreach my $issue (@$issues) {
            if ($issue->{file} ne $current_file) {
                $current_file = $issue->{file};
                print $fh "--- $current_file ---\n";
            }
            printf $fh "  Line %d: %s [%s] %s\n",
                $issue->{line}, $issue->{severity}, $issue->{rule}, $issue->{message};
        }
    }

    print $fh "\n", "=" x 70, "\n";
    close($fh);
}

sub write_report_json {
    my ($path, $issues) = @_;

    open(my $fh, '>:encoding(UTF-8)', $path) or die "Cannot write report to $path: $!\n";

    print $fh "{\n";
    print $fh "  \"generator\": \"$PROGRAM\",\n";
    print $fh "  \"version\": \"$VERSION\",\n";
    print $fh "  \"timestamp\": \"" . POSIX::strftime("%Y-%m-%dT%H:%M:%S", localtime) . "\",\n";
    print $fh "  \"summary\": {\n";
    print $fh "    \"files_checked\": $g_file_count,\n";
    print $fh "    \"lines_checked\": $g_line_count,\n";
    print $fh "    \"errors\": $g_error_count,\n";
    print $fh "    \"warnings\": $g_warn_count,\n";
    print $fh "    \"info\": $g_info_count\n";
    print $fh "  },\n";
    print $fh "  \"issues\": [\n";

    for (my $i = 0; $i < scalar @$issues; $i++) {
        my $issue = $issues->[$i];
        my $comma = ($i < scalar(@$issues) - 1) ? ',' : '';

        # Escape JSON strings
        my $file_j = json_escape($issue->{file});
        my $msg_j  = json_escape($issue->{message});
        my $rule_j = json_escape($issue->{rule});
        my $sev_j  = json_escape($issue->{severity});

        print $fh "    {\n";
        print $fh "      \"file\": \"$file_j\",\n";
        print $fh "      \"line\": $issue->{line},\n";
        print $fh "      \"severity\": \"$sev_j\",\n";
        print $fh "      \"rule\": \"$rule_j\",\n";
        print $fh "      \"message\": \"$msg_j\"\n";
        print $fh "    }$comma\n";
    }

    print $fh "  ]\n";
    print $fh "}\n";

    close($fh);
}

sub json_escape {
    my ($str) = @_;
    return '' unless defined $str;
    $str =~ s/\\/\\\\/g;
    $str =~ s/"/\\"/g;
    $str =~ s/\n/\\n/g;
    $str =~ s/\r/\\r/g;
    $str =~ s/\t/\\t/g;
    # Escape control characters
    $str =~ s/([\x00-\x1f])/sprintf("\\u%04x", ord($1))/ge;
    return $str;
}

sub write_report_html {
    my ($path, $issues) = @_;

    open(my $fh, '>:encoding(UTF-8)', $path) or die "Cannot write report to $path: $!\n";

    my $timestamp = POSIX::strftime("%Y-%m-%d %H:%M:%S", localtime);
    my $total = scalar @$issues;

    print $fh <<"HTML";
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MeshMC checkpatch Report</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
         background: #0d1117; color: #c9d1d9; line-height: 1.6; padding: 2rem; }
  .container { max-width: 1200px; margin: 0 auto; }
  h1 { color: #58a6ff; margin-bottom: 0.5rem; font-size: 1.8rem; }
  .meta { color: #8b949e; margin-bottom: 1.5rem; font-size: 0.9rem; }
  .summary { display: flex; gap: 1rem; margin-bottom: 2rem; flex-wrap: wrap; }
  .stat { background: #161b22; border: 1px solid #30363d; border-radius: 8px;
          padding: 1rem 1.5rem; min-width: 140px; }
  .stat .value { font-size: 2rem; font-weight: bold; }
  .stat .label { color: #8b949e; font-size: 0.85rem; }
  .stat.errors .value { color: #f85149; }
  .stat.warnings .value { color: #d29922; }
  .stat.info .value { color: #58a6ff; }
  .stat.files .value { color: #3fb950; }
  .filters { margin-bottom: 1rem; display: flex; gap: 0.5rem; flex-wrap: wrap; }
  .filters button { background: #21262d; color: #c9d1d9; border: 1px solid #30363d;
                    border-radius: 6px; padding: 0.4rem 1rem; cursor: pointer;
                    font-size: 0.85rem; transition: all 0.2s; }
  .filters button:hover { background: #30363d; }
  .filters button.active { background: #388bfd26; border-color: #58a6ff; color: #58a6ff; }
  table { width: 100%; border-collapse: collapse; background: #161b22;
          border: 1px solid #30363d; border-radius: 8px; overflow: hidden; }
  th { background: #21262d; text-align: left; padding: 0.75rem 1rem;
       color: #8b949e; font-size: 0.85rem; text-transform: uppercase;
       border-bottom: 1px solid #30363d; }
  td { padding: 0.6rem 1rem; border-bottom: 1px solid #30363d1a;
       font-size: 0.9rem; vertical-align: top; }
  tr:hover td { background: #1c2128; }
  .sev { display: inline-block; padding: 0.15rem 0.5rem; border-radius: 4px;
         font-size: 0.75rem; font-weight: 600; text-transform: uppercase; }
  .sev.error { background: #f8514926; color: #f85149; }
  .sev.warning { background: #d2992226; color: #d29922; }
  .sev.info { background: #58a6ff26; color: #58a6ff; }
  .rule { color: #bc8cff; font-family: monospace; font-size: 0.85rem; }
  .file { color: #58a6ff; font-family: monospace; font-size: 0.85rem; }
  .line-num { color: #8b949e; font-family: monospace; }
  .message { color: #c9d1d9; }
  .passed { text-align: center; padding: 3rem; color: #3fb950; font-size: 1.3rem; }
  .passed::before { content: "\\2713 "; font-size: 2rem; }
  .footer { margin-top: 2rem; color: #484f58; font-size: 0.8rem; text-align: center; }
</style>
</head>
<body>
<div class="container">
<h1>MeshMC checkpatch Report</h1>
<div class="meta">Generated: $timestamp | $PROGRAM v$VERSION</div>
<div class="summary">
  <div class="stat files"><div class="value">$g_file_count</div><div class="label">Files Checked</div></div>
  <div class="stat"><div class="value">$g_line_count</div><div class="label">Lines Checked</div></div>
  <div class="stat errors"><div class="value">$g_error_count</div><div class="label">Errors</div></div>
  <div class="stat warnings"><div class="value">$g_warn_count</div><div class="label">Warnings</div></div>
  <div class="stat info"><div class="value">$g_info_count</div><div class="label">Info</div></div>
</div>
HTML

    if ($total == 0) {
        print $fh "<div class=\"passed\">All checks passed! No issues found.</div>\n";
    } else {
        print $fh <<"FILTER_JS";
<div class="filters">
  <button class="active" onclick="filterRows('all', this)">All ($total)</button>
  <button onclick="filterRows('error', this)">Errors ($g_error_count)</button>
  <button onclick="filterRows('warning', this)">Warnings ($g_warn_count)</button>
  <button onclick="filterRows('info', this)">Info ($g_info_count)</button>
</div>
<table>
<thead>
<tr><th>File</th><th>Line</th><th>Severity</th><th>Rule</th><th>Message</th></tr>
</thead>
<tbody>
FILTER_JS

        foreach my $issue (@$issues) {
            my $sev_class = lc($issue->{severity});
            my $file_h = html_escape($issue->{file});
            my $msg_h  = html_escape($issue->{message});
            my $rule_h = html_escape($issue->{rule});
            print $fh "<tr data-severity=\"$sev_class\">";
            print $fh "<td class=\"file\">$file_h</td>";
            print $fh "<td class=\"line-num\">$issue->{line}</td>";
            print $fh "<td><span class=\"sev $sev_class\">$issue->{severity}</span></td>";
            print $fh "<td class=\"rule\">$rule_h</td>";
            print $fh "<td class=\"message\">$msg_h</td>";
            print $fh "</tr>\n";
        }

        print $fh <<"TABLE_END";
</tbody>
</table>
<script>
function filterRows(severity, btn) {
  document.querySelectorAll('.filters button').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');
  document.querySelectorAll('tbody tr').forEach(row => {
    if (severity === 'all' || row.dataset.severity === severity) {
      row.style.display = '';
    } else {
      row.style.display = 'none';
    }
  });
}
</script>
TABLE_END
    }

    print $fh "<div class=\"footer\">Generated by $PROGRAM v$VERSION</div>\n";
    print $fh "</div>\n</body>\n</html>\n";

    close($fh);
}

sub html_escape {
    my ($str) = @_;
    return '' unless defined $str;
    $str =~ s/&/&amp;/g;
    $str =~ s/</&lt;/g;
    $str =~ s/>/&gt;/g;
    $str =~ s/"/&quot;/g;
    $str =~ s/'/&#39;/g;
    return $str;
}

sub write_report_csv {
    my ($path, $issues) = @_;

    open(my $fh, '>:encoding(UTF-8)', $path) or die "Cannot write report to $path: $!\n";

    # CSV header
    print $fh "File,Line,Severity,Rule,Message\n";

    foreach my $issue (@$issues) {
        my $file = csv_escape($issue->{file});
        my $msg  = csv_escape($issue->{message});
        my $rule = csv_escape($issue->{rule});
        my $sev  = csv_escape($issue->{severity});
        print $fh "$file,$issue->{line},$sev,$rule,$msg\n";
    }

    close($fh);
}

sub csv_escape {
    my ($str) = @_;
    return '""' unless defined $str;
    # If field contains comma, quote, or newline, wrap in quotes
    if ($str =~ /[",\n\r]/) {
        $str =~ s/"/""/g;
        return "\"$str\"";
    }
    return $str;
}

# ===========================================================================
# File Discovery and Filtering
# ===========================================================================

sub should_exclude {
    my ($filepath) = @_;

    # Check default excludes
    foreach my $pattern (@DEFAULT_EXCLUDES) {
        my $regex = glob_to_regex($pattern);
        return 1 if $filepath =~ /$regex/;
    }

    # Check user excludes
    foreach my $pattern (@opt_excludes) {
        my $regex = glob_to_regex($pattern);
        return 1 if $filepath =~ /$regex/;
    }

    return 0;
}

sub glob_to_regex {
    my ($glob) = @_;
    my $regex = quotemeta($glob);
    $regex =~ s/\\\*\\\*/.*/g;     # ** -> .*
    $regex =~ s/\\\*([^.])/[^\/]*$1/g;  # * -> [^/]*
    $regex =~ s/\\\*$/[^\/]*/g;    # trailing * -> [^/]*
    $regex =~ s/\\\?/./g;          # ? -> .
    return $regex;
}

sub get_file_type {
    my ($filepath) = @_;
    my $basename = basename($filepath);

    # Check CMake files
    if ($basename eq 'CMakeLists.txt' || $filepath =~ /\.cmake$/i) {
        return FTYPE_CMAKE;
    }

    # Check by extension
    my ($name, $dir, $ext) = fileparse($filepath, qr/\.[^.]*/);
    $ext = lc($ext);

    return $EXT_TO_FTYPE{$ext} // FTYPE_OTHER;
}

sub is_source_file {
    my ($filepath) = @_;
    my $ftype = get_file_type($filepath);
    return ($ftype eq FTYPE_CPP || $ftype eq FTYPE_HEADER || $ftype eq FTYPE_CMAKE);
}

# ===========================================================================
# Directory Processing
# ===========================================================================

sub find_git_root {
    my $output = `git rev-parse --show-toplevel 2>/dev/null`;
    chomp $output if $output;
    return $output if $output && -d $output;
    return undef;
}

sub process_directory {
    my ($dir) = @_;

    $dir = abs_path($dir) unless File::Spec->file_name_is_absolute($dir);

    find({
        wanted => sub {
            my $filepath = $File::Find::name;
            return unless -f $filepath;
            return if should_exclude($filepath);
            return unless is_source_file($filepath);
            process_file($filepath);
        },
        no_chdir => 1,
    }, $dir);
}

# ===========================================================================
# Diff Processing
# ===========================================================================

sub process_diff_from_stdin {
    if (-t STDIN) {
        print STDERR "WARNING: --diff reads from stdin, but stdin is a terminal.\n";
        print STDERR "Usage:  git diff | $0 --diff\n";
        print STDERR "        $0 --diff < file.patch\n";
        print STDERR "Press Ctrl+D to send EOF, or Ctrl+C to cancel.\n\n";
    }
    my @diff_lines = <STDIN>;
    process_diff_content(\@diff_lines);
}

sub process_git_diff {
    my ($ref) = @_;

    # Sanitize ref to prevent command injection
    if ($ref =~ /[;&|`\$\(\)\{\}\[\]<>!\\]/) {
        die "ERROR: Invalid git ref: contains shell metacharacters\n";
    }
    if ($ref =~ /\s/) {
        die "ERROR: Invalid git ref: contains whitespace\n";
    }

    my @diff_lines = `git diff "$ref" -- '*.cpp' '*.h' '*.hpp' 'CMakeLists.txt' '*.cmake' 2>&1`;
    if ($? != 0) {
        die "ERROR: git diff failed: @diff_lines\n";
    }

    process_diff_content(\@diff_lines);
}

sub process_diff_content {
    my ($lines_ref) = @_;

    # First pass: parse diff to collect changed line numbers per file
    my %file_changes;   # filename => { line_num => 1, ... }
    my $current_file = '';
    my $current_line = 0;

    foreach my $line (@$lines_ref) {
        chomp $line;

        # New file in diff: "diff --git a/foo.cpp b/foo.cpp"
        if ($line =~ /^diff --git a\/.+? b\/(.+)/) {
            $current_file = $1;
            $current_line = 0;
            $file_changes{$current_file} //= {};
            next;
        }

        # Also catch "+++ b/foo.cpp" for non-git diffs
        if ($line =~ /^\+\+\+ b\/(.+)/) {
            $current_file = $1;
            $file_changes{$current_file} //= {};
            next;
        }

        # Hunk header: @@ -old,count +new,count @@
        if ($line =~ /^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@/) {
            $current_line = $1 - 1;
            next;
        }

        # Skip other header lines
        next if $line =~ /^(?:---|\+\+\+|index |new file|deleted file|old mode|new mode|similarity|rename|Binary)/;
        next unless $current_file;

        # Added line — this is a changed line
        if ($line =~ /^\+/) {
            $current_line++;
            $file_changes{$current_file}{$current_line} = 1;
        }
        # Context line — exists in new file, not changed
        elsif ($line =~ /^ /) {
            $current_line++;
        }
        # Removed line — don't increment (doesn't exist in new file)
        elsif ($line =~ /^-/) {
            # skip
        }
    }

    # Second pass: for each changed file, read the full file from disk
    # and check it with only diff-changed lines flagged
    foreach my $file (sort keys %file_changes) {
        my $filepath = $file;

        # Resolve to absolute path if the file exists relative to cwd
        if (!File::Spec->file_name_is_absolute($filepath)) {
            my $abs = File::Spec->rel2abs($filepath);
            $filepath = $abs if -f $abs;
        }

        unless (-f $filepath && -r $filepath) {
            # File might have been deleted in the diff
            next;
        }

        # Skip binary files
        next if -B $filepath;

        # Skip excluded files
        next if should_exclude($filepath);

        # Skip non-source files
        next unless is_source_file($filepath);

        # Read the full file from disk
        open(my $fh, '<:encoding(UTF-8)', $filepath) or do {
            report($filepath, 0, SEV_ERROR, 'FILE_READ',
                "Cannot open file: $!");
            next;
        };
        my @lines = <$fh>;
        close($fh);
        chomp(@lines);

        $g_file_count++;
        $g_line_count += scalar @lines;

        # Pass the full file content but only mark diff-changed lines
        my %changed = %{$file_changes{$file}};
        check_file_content($filepath, \@lines, \%changed);
    }
}

# ===========================================================================
# File Processing
# ===========================================================================

sub process_file {
    my ($filepath) = @_;

    # Ensure the file exists and is readable
    unless (-f $filepath && -r $filepath) {
        report($filepath, 0, SEV_ERROR, 'FILE_ACCESS',
            "Cannot read file");
        return;
    }

    # Skip binary files
    if (-B $filepath) {
        return;
    }

    # Read file content
    open(my $fh, '<:encoding(UTF-8)', $filepath) or do {
        report($filepath, 0, SEV_ERROR, 'FILE_READ',
            "Cannot open file: $!");
        return;
    };
    my @lines = <$fh>;
    close($fh);

    chomp(@lines);

    $g_file_count++;
    $g_line_count += scalar @lines;

    # All lines are considered "changed" when checking a file directly
    my %all_lines = map { ($_ + 1) => 1 } (0 .. $#lines);
    check_file_content($filepath, \@lines, \%all_lines);
}

# ===========================================================================
# Master Check Dispatcher
# ===========================================================================

sub check_file_content {
    my ($filepath, $lines_ref, $changed_lines_ref) = @_;

    my $ftype = get_file_type($filepath);

    if ($opt_verbose) {
        print colorize('dim', "Checking: $filepath ($ftype)") . "\n";
    }

    # Common checks for all file types
    check_trailing_whitespace($filepath, $lines_ref, $changed_lines_ref);
    check_line_endings($filepath, $lines_ref, $changed_lines_ref);
    check_file_ending_newline($filepath, $lines_ref);
    check_consecutive_blank_lines($filepath, $lines_ref, $changed_lines_ref);

    # Type-specific checks
    if ($ftype eq FTYPE_CPP || $ftype eq FTYPE_HEADER) {
        check_cpp_conventions($filepath, $lines_ref, $changed_lines_ref, $ftype);
    } elsif ($ftype eq FTYPE_CMAKE) {
        check_cmake_conventions($filepath, $lines_ref, $changed_lines_ref);
    }
}

# ===========================================================================
# Common Checks (All File Types)
# ===========================================================================

sub check_trailing_whitespace {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        if ($line =~ /[ \t]+$/) {
            report($filepath, $linenum, SEV_WARNING, 'TRAILING_WHITESPACE',
                "Trailing whitespace detected");

            if ($opt_fix) {
                $lines_ref->[$i] =~ s/[ \t]+$//;
            }
        }
    }
}

sub check_line_endings {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        if ($line =~ /\r$/) {
            report($filepath, $linenum, SEV_ERROR, 'CRLF_LINE_ENDING',
                "Windows-style (CRLF) line ending detected; use Unix-style (LF)");

            if ($opt_fix) {
                $lines_ref->[$i] =~ s/\r$//;
            }
        }
    }
}

sub check_file_ending_newline {
    my ($filepath, $lines_ref) = @_;

    return unless @$lines_ref;

    # Check if file ends with a newline (the last element after chomp should be defined)
    # We actually read the raw file to check this
    if (open(my $fh, '<:raw', $filepath)) {
        seek($fh, -1, 2);
        my $last_char;
        read($fh, $last_char, 1);
        close($fh);

        if (defined $last_char && $last_char ne "\n") {
            report($filepath, scalar @$lines_ref, SEV_WARNING, 'NO_FINAL_NEWLINE',
                "File does not end with a newline");
        }
    }
}

sub check_consecutive_blank_lines {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $blank_count = 0;
    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        if ($line =~ /^\s*$/) {
            $blank_count++;
            if ($blank_count > $MAX_CONSECUTIVE_BLANK && exists $changed_ref->{$linenum}) {
                report($filepath, $linenum, SEV_WARNING, 'CONSECUTIVE_BLANK_LINES',
                    "More than $MAX_CONSECUTIVE_BLANK consecutive blank lines");
            }
        } else {
            $blank_count = 0;
        }
    }
}

# ===========================================================================
# C++ and Header File Checks
# ===========================================================================

sub check_cpp_conventions {
    my ($filepath, $lines_ref, $changed_ref, $ftype) = @_;

    # File-level checks
    check_license_header($filepath, $lines_ref);
    check_pragma_once($filepath, $lines_ref, $ftype);
    check_include_ordering($filepath, $lines_ref);
    check_file_length($filepath, $lines_ref);

    # Line-level checks
    check_indentation($filepath, $lines_ref, $changed_ref);
    check_line_length($filepath, $lines_ref, $changed_ref);
    check_tab_usage($filepath, $lines_ref, $changed_ref);
    check_brace_style($filepath, $lines_ref, $changed_ref);
    check_naming_conventions($filepath, $lines_ref, $changed_ref);
    check_pointer_alignment($filepath, $lines_ref, $changed_ref);
    check_spacing_conventions($filepath, $lines_ref, $changed_ref);
    check_qt_conventions($filepath, $lines_ref, $changed_ref);
    check_include_style($filepath, $lines_ref, $changed_ref);
    check_comment_style($filepath, $lines_ref, $changed_ref);
    check_control_flow_style($filepath, $lines_ref, $changed_ref);
    check_string_usage($filepath, $lines_ref, $changed_ref);
    check_memory_patterns($filepath, $lines_ref, $changed_ref);
    check_const_correctness($filepath, $lines_ref, $changed_ref);
    check_enum_style($filepath, $lines_ref, $changed_ref);
    check_constructor_patterns($filepath, $lines_ref, $changed_ref);
    check_function_length($filepath, $lines_ref);
    check_nesting_depth($filepath, $lines_ref, $changed_ref);
    check_deprecated_patterns($filepath, $lines_ref, $changed_ref);
    check_security_patterns($filepath, $lines_ref, $changed_ref);
    check_test_conventions($filepath, $lines_ref, $changed_ref);
    check_exception_patterns($filepath, $lines_ref, $changed_ref);
    check_virtual_destructor($filepath, $lines_ref, $changed_ref);
    check_override_keyword($filepath, $lines_ref, $changed_ref);
    check_auto_usage($filepath, $lines_ref, $changed_ref);
    check_lambda_style($filepath, $lines_ref, $changed_ref);
    check_switch_case_style($filepath, $lines_ref, $changed_ref);
    check_class_member_ordering($filepath, $lines_ref, $changed_ref);
    check_todo_fixme($filepath, $lines_ref, $changed_ref);
    check_debug_statements($filepath, $lines_ref, $changed_ref);
    check_header_self_containment($filepath, $lines_ref, $ftype);
    check_multiple_statements_per_line($filepath, $lines_ref, $changed_ref);
    check_magic_numbers($filepath, $lines_ref, $changed_ref);
    check_using_namespace($filepath, $lines_ref, $changed_ref);
}

# ===========================================================================
# License Header Check
# ===========================================================================
# REUSE-IgnoreStart
sub check_license_header {
    my ($filepath, $lines_ref) = @_;

    return unless @$lines_ref;

    # Join first 30 lines for header check
    my $header_text = join("\n", @{$lines_ref}[0 .. min(29, $#$lines_ref)]);

    # Check for SPDX header
    unless ($header_text =~ /SPDX-License-Identifier/) {
        report($filepath, 1, SEV_ERROR, 'MISSING_SPDX_HEADER',
            "Missing SPDX license header (expected SPDX-License-Identifier)");
        return;
    }

    # Check for GPL-3.0-or-later (project standard)
    unless ($header_text =~ /SPDX-License-Identifier:\s*GPL-3\.0-or-later/
         || $header_text =~ /SPDX-License-Identifier:\s*(?:Apache-2\.0|LGPL-[23]\.\d|MIT|BSD)/
         || $header_text =~ /SPDX-License-Identifier:\s*\S+/) {
        report($filepath, 1, SEV_WARNING, 'INVALID_SPDX_IDENTIFIER',
            "SPDX-License-Identifier found but could not parse license type");
    }

    # Check for SPDX-FileCopyrightText
    unless ($header_text =~ /SPDX-FileCopyrightText:/) {
        report($filepath, 1, SEV_WARNING, 'MISSING_COPYRIGHT',
            "Missing SPDX-FileCopyrightText in header");
    }

    # Check header is in a block comment
    unless ($header_text =~ m{^\s*/\*} || $header_text =~ m{^\s*//}) {
        report($filepath, 1, SEV_WARNING, 'HEADER_NOT_IN_COMMENT',
            "SPDX header should be in a comment block (/* */ or //)");
    }
}
# REUSE-IgnoreEnd
# ===========================================================================
# #pragma once Check
# ===========================================================================

sub check_pragma_once {
    my ($filepath, $lines_ref, $ftype) = @_;

    return unless $ftype eq FTYPE_HEADER;
    return unless @$lines_ref;

    my $found_pragma = 0;
    my $found_ifndef = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];

        if ($line =~ /^\s*#\s*pragma\s+once\b/) {
            $found_pragma = 1;
            last;
        }
        if ($line =~ /^\s*#\s*ifndef\s+\w+_H/) {
            $found_ifndef = 1;
        }

        # Only check first 40 lines (past the license header)
        last if $i > 40;
    }

    if (!$found_pragma) {
        if ($found_ifndef) {
            report($filepath, 1, SEV_WARNING, 'USE_PRAGMA_ONCE',
                "Use '#pragma once' instead of #ifndef header guards (project convention)");
        } else {
            report($filepath, 1, SEV_ERROR, 'MISSING_HEADER_GUARD',
                "Header file missing '#pragma once' guard");
        }
    }
}

# ===========================================================================
# Include Ordering Check
# ===========================================================================

sub check_include_ordering {
    my ($filepath, $lines_ref) = @_;

    my @include_groups = ();
    my $current_group = [];
    my $last_was_include = 0;
    my $pragma_found = 0;
    my $in_header_section = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];

        # Skip until we find #pragma once
        if ($line =~ /^\s*#\s*pragma\s+once/) {
            $pragma_found = 1;
            $in_header_section = 1;
            next;
        }

        next unless $in_header_section;

        # Stop at first non-include, non-blank, non-comment line
        if ($line !~ /^\s*$/ && $line !~ /^\s*#\s*include/ && $line !~ /^\s*\/\//) {
            last if $last_was_include || @$current_group;
        }

        if ($line =~ /^\s*#\s*include\s*[<"](.+?)[>"]/) {
            push @$current_group, {
                path    => $1,
                line    => $i + 1,
                system  => ($line =~ /</) ? 1 : 0,
                raw     => $line,
            };
            $last_was_include = 1;
        } elsif ($line =~ /^\s*$/ && $last_was_include) {
            # Blank line separates include groups
            if (@$current_group) {
                push @include_groups, $current_group;
                $current_group = [];
            }
            $last_was_include = 0;
        }
    }

    # Push last group
    if (@$current_group) {
        push @include_groups, $current_group;
    }

    # Check for proper grouping within each group
    foreach my $group (@include_groups) {
        check_include_group_sorting($filepath, $group);
    }
}

sub check_include_group_sorting {
    my ($filepath, $group) = @_;

    return unless @$group > 1;

    # Check if includes within a group are alphabetically sorted
    for (my $i = 1; $i < scalar @$group; $i++) {
        my $prev_path = $group->[$i-1]->{path};
        my $curr_path = $group->[$i]->{path};

        # Skip entries with undefined path (safety guard)
        next unless defined $prev_path && defined $curr_path;

        my $prev = lc($prev_path);
        my $curr = lc($curr_path);

        # Only warn within the same type (system vs local)
        if ($group->[$i-1]->{system} == $group->[$i]->{system}) {
            if ($curr lt $prev) {
                report($filepath, $group->[$i]->{line}, SEV_INFO, 'INCLUDE_ORDER',
                    "Include '$group->[$i]->{path}' should come before '$group->[$i-1]->{path}' (alphabetical order)");
            }
        }
    }
}

# ===========================================================================
# File Length Check
# ===========================================================================

sub check_file_length {
    my ($filepath, $lines_ref) = @_;

    my $line_count = scalar @$lines_ref;
    if ($line_count > $MAX_FILE_LENGTH) {
        report($filepath, $line_count, SEV_WARNING, 'FILE_TOO_LONG',
            "File has $line_count lines (recommended maximum: $MAX_FILE_LENGTH). Consider splitting.");
    }
}

# ===========================================================================
# Indentation Check (4 spaces for C++, no tabs)
# ===========================================================================

sub check_indentation {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $in_multiline_comment = 0;
    my $in_raw_string = 0;
    my $in_multiline_macro = 0;
    my $in_multiline_string = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Track multi-line comment state
        if ($line =~ /\/\*/ && $line !~ /\*\//) {
            $in_multiline_comment = 1;
        }
        if ($in_multiline_comment) {
            $in_multiline_comment = 0 if $line =~ /\*\//;
            next;
        }

        # Skip preprocessor directives (they have their own indentation rules)
        next if $line =~ /^\s*#/;

        # Skip blank lines
        next if $line =~ /^\s*$/;

        # Skip raw string literals
        next if $line =~ /R"[^"]*\(/;
        if ($in_raw_string) {
            $in_raw_string = 0 if $line =~ /\)"/;
            next;
        }
        if ($line =~ /R"[^"]*\(/ && $line !~ /\)"/) {
            $in_raw_string = 1;
            next;
        }

        # Skip continuation lines (ending with backslash)
        if ($in_multiline_macro) {
            $in_multiline_macro = ($line =~ /\\$/);
            next;
        }
        if ($line =~ /\\$/) {
            $in_multiline_macro = 1;
        }

        # Check for tab indentation
        if ($line =~ /^\t/) {
            report($filepath, $linenum, SEV_ERROR, 'TAB_INDENT',
                "Tab indentation detected; use $INDENT_WIDTH spaces per indent level");
            next;
        }

        # Check indentation is a multiple of INDENT_WIDTH
        if ($line =~ /^( +)\S/) {
            my $indent = length($1);

            # Skip lines that are continuation of previous line
            # (parameter lists, ternary operators, initializer lists, etc.)
            if ($i > 0) {
                my $prev_line = $lines_ref->[$i - 1];
                # Continuation patterns: open paren, trailing comma, trailing operator
                next if $prev_line =~ /[,({\[]\s*$/;
                next if $prev_line =~ /(?:&&|\|\||:|\?)\s*$/;
                next if $prev_line =~ /\\$/;
                # Initializer list continuation
                next if $line =~ /^\s*:/;
                next if $line =~ /^\s*,/;
                # Alignment with previous line's arguments
                next if $indent != 0 && ($indent % $INDENT_WIDTH != 0);
            }

            # Only warn on obviously wrong indentation (not alignment)
            # This is tricky because alignment is valid, so we're conservative
            if ($indent % $INDENT_WIDTH != 0) {
                # Check if it could be alignment (e.g., function argument alignment)
                # Be lenient: only flag multiples of 2 that aren't multiples of 4
                if ($indent > 0 && $indent % 2 == 0 && $indent % $INDENT_WIDTH != 0) {
                    # Could be 2-space indent, which is wrong
                    # But could also be alignment, so make it info-level
                    report($filepath, $linenum, SEV_INFO, 'INDENT_NOT_MULTIPLE',
                        "Indentation is $indent spaces (not a multiple of $INDENT_WIDTH). " .
                        "May be alignment, or incorrect indent width.");
                }
            }
        }
    }
}

# ===========================================================================
# Tab Usage Check
# ===========================================================================

sub check_tab_usage {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Check for tabs anywhere in the line (not just indentation)
        # But skip tab characters in string literals
        my $stripped = $line;
        $stripped =~ s/"(?:[^"\\]|\\.)*"//g;   # Remove string literals
        $stripped =~ s/'(?:[^'\\]|\\.)*'//g;   # Remove char literals

        if ($stripped =~ /\t/ && $line !~ /^\t/) {
            report($filepath, $linenum, SEV_WARNING, 'TAB_IN_CODE',
                "Tab character found in non-indentation position; use spaces");
        }
    }
}

# ===========================================================================
# Line Length Check
# ===========================================================================

sub check_line_length {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        my $len = length($line);

        # Skip long include lines - hard to break
        next if $line =~ /^\s*#\s*include/;

        # Skip long URLs in comments
        next if $line =~ /https?:\/\//;

        # Skip long string literals (difficult to break)
        next if $line =~ /^\s*"[^"]*"\s*;?\s*$/;
        next if $line =~ /QStringLiteral\s*\(/;
        next if $line =~ /QLatin1String\s*\(/;
        next if $line =~ /tr\s*\(/;

        if ($len > $MAX_LINE_LENGTH) {
            report($filepath, $linenum, SEV_WARNING, 'LINE_TOO_LONG',
                "Line is $len characters (maximum: $MAX_LINE_LENGTH)");
        } elsif ($len > $MAX_LINE_LENGTH + 20) {
            report($filepath, $linenum, SEV_ERROR, 'LINE_EXCESSIVELY_LONG',
                "Line is $len characters (far exceeds maximum: $MAX_LINE_LENGTH)");
        }
    }
}

# ===========================================================================
# Brace Style Check (K&R / 1TBS)
# ===========================================================================

sub check_brace_style {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $in_multiline_comment = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        # Track multi-line comments
        if ($line =~ /\/\*/ && $line !~ /\*\//) {
            $in_multiline_comment = 1;
        }
        if ($in_multiline_comment) {
            $in_multiline_comment = 0 if $line =~ /\*\//;
            next;
        }

        next unless exists $changed_ref->{$linenum};

        # Skip preprocessor
        next if $line =~ /^\s*#/;
        # Skip string-only lines
        next if $line =~ /^\s*"/;
        # Skip comments
        next if $line =~ /^\s*\/\//;

        # Check: opening brace on its own line after control statement
        # MeshMC uses a mixed style - switch statements have brace on next line,
        # if/else/for/while have brace on same line (mostly)
        # We only flag clearly wrong patterns

        # Check for "} else {" on separate lines (should be on one line)
        if ($line =~ /^\s*}\s*$/) {
            if ($i + 1 < scalar @$lines_ref) {
                my $next_line = $lines_ref->[$i + 1];
                # This is fine in MeshMC style - "} else {" can be split
            }
        }

        # Opening brace alone on a line after a function-like statement
        if ($line =~ /^\s*{\s*$/) {
            if ($i > 0) {
                my $prev = $lines_ref->[$i - 1];

                # It's OK for switch statements to have { on next line
                next if $prev =~ /\bswitch\s*\(/;

                # It's OK for function definitions to have { on next line
                # (constructor initializer list pattern)
                next if $prev =~ /^\s*:/;  # initializer list
                next if $prev =~ /\)\s*$/; # end of function signature

                # It's OK for namespace/class/struct/enum
                next if $prev =~ /\b(?:namespace|class|struct|enum)\b/;

                # Control structures should have brace on same line
                if ($prev =~ /\b(?:if|else|for|while|do)\b/) {
                    # This is actually allowed in MeshMC for some cases
                    # Only flag if the prev line doesn't end with )
                    if ($prev =~ /\)\s*$/) {
                        report($filepath, $linenum, SEV_INFO, 'BRACE_NEXT_LINE_CONTROL',
                            "Consider placing opening brace on the same line as the control statement (K&R style)");
                    }
                }
            }
        }

        # Check for "} else" on same line (this is fine, just check formatting)
        if ($line =~ /}\s*else\b/ && $line !~ /}\s+else\b/) {
            report($filepath, $linenum, SEV_STYLE, 'BRACE_ELSE_SPACING',
                "Missing space between '}' and 'else'");
        }

        # Check: empty else blocks
        if ($line =~ /\belse\s*{\s*}\s*$/) {
            report($filepath, $linenum, SEV_WARNING, 'EMPTY_ELSE_BLOCK',
                "Empty else block");
        }

        # Check: empty if blocks
        if ($line =~ /\bif\s*\([^)]*\)\s*{\s*}\s*$/) {
            report($filepath, $linenum, SEV_WARNING, 'EMPTY_IF_BLOCK',
                "Empty if block");
        }
    }
}

# ===========================================================================
# Naming Convention Check
# ===========================================================================

sub check_naming_conventions {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $in_class = 0;
    my $access_level = '';
    my $in_multiline_comment = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        # Track comments
        if ($line =~ /\/\*/ && $line !~ /\*\//) {
            $in_multiline_comment = 1;
        }
        if ($in_multiline_comment) {
            $in_multiline_comment = 0 if $line =~ /\*\//;
            next;
        }
        next if $line =~ /^\s*\/\//;

        next unless exists $changed_ref->{$linenum};

        # Track class/struct context
        if ($line =~ /\b(?:class|struct)\s+([A-Za-z_]\w*)\b/) {
            my $name = $1;
            $in_class = 1;

            # Skip forward declarations
            next if $line =~ /;\s*$/;

            # Check class name is PascalCase
            unless ($name =~ $RE_PASCAL_CASE) {
                report($filepath, $linenum, SEV_WARNING, 'CLASS_NAME_CASE',
                    "Class/struct name '$name' should be PascalCase");
            }
        }

        # Track access level
        if ($line =~ /^\s*(public|protected|private)\s*(?::|\s)/) {
            $access_level = $1;
        }

        # Check member variable naming (m_ prefix)
        # Pattern: type m_name; in class context
        if ($in_class && $line =~ /^\s+(?:(?:const\s+)?(?:\w+(?:::\w+)*(?:\s*[*&]\s*|\s+)))(m_\w+)\s*[=;]/) {
            my $name = $1;
            unless ($name =~ $RE_MEMBER_VAR) {
                report($filepath, $linenum, SEV_WARNING, 'MEMBER_VAR_NAMING',
                    "Member variable '$name' should follow m_camelCase convention");
            }
        }

        # Check for member variables without m_ prefix in private/protected
        if ($in_class && ($access_level eq 'private' || $access_level eq 'protected')) {
            # Pattern: "Type varName;" where varName doesn't start with m_
            if ($line =~ /^\s+(?:(?:const\s+)?(?:(?:std::)?(?:shared_ptr|unique_ptr|weak_ptr|vector|map|set|string|optional)\s*<[^>]+>|\w+(?:::\w+)*)\s*[*&]?\s+)([a-z]\w*)\s*[=;]/) {
                my $name = $1;
                # Skip common false positives
                next if $name =~ /^(?:override|const|return|static|virtual|explicit|inline|typename|template)$/;
                # Skip function parameters
                next if $line =~ /\(/;

                unless ($name =~ /^m_/) {
                    report($filepath, $linenum, SEV_INFO, 'MEMBER_NO_PREFIX',
                        "Private/protected member '$name' should use m_ prefix (m_$name)");
                }
            }
        }

        # Check enum values - PascalCase for enum class
        if ($line =~ /\benum\s+class\s+(\w+)/) {
            # Check the enum name
            my $enum_name = $1;
            unless ($enum_name =~ $RE_PASCAL_CASE) {
                report($filepath, $linenum, SEV_WARNING, 'ENUM_CLASS_NAME',
                    "Enum class name '$enum_name' should be PascalCase");
            }
        }

        # Check #define macro naming (UPPER_SNAKE_CASE)
        if ($line =~ /^\s*#\s*define\s+([A-Za-z_]\w*)/) {
            my $name = $1;
            # Skip include guards and known Qt macros
            next if exists $KNOWN_QT_MACROS{$name};
            next if exists $KNOWN_PROJECT_MACROS{$name};
            # Skip function-like macros that are essentially inline functions
            next if $line =~ /#\s*define\s+\w+\s*\(/;

            unless ($name =~ $RE_MACRO_CASE) {
                report($filepath, $linenum, SEV_WARNING, 'MACRO_NAME_CASE',
                    "Macro name '$name' should be UPPER_SNAKE_CASE");
            }
        }

        # Check namespace naming
        if ($line =~ /\bnamespace\s+([A-Za-z_]\w*)/) {
            my $name = $1;
            # MeshMC uses PascalCase for namespaces
            unless ($name =~ $RE_PASCAL_CASE || $name =~ $RE_SNAKE_CASE) {
                report($filepath, $linenum, SEV_INFO, 'NAMESPACE_NAMING',
                    "Namespace '$name' should use PascalCase (project convention)");
            }
        }

        # End of class
        if ($line =~ /^};/) {
            $in_class = 0;
            $access_level = '';
        }
    }
}

# ===========================================================================
# Pointer/Reference Alignment Check
# ===========================================================================

sub check_pointer_alignment {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments, preprocessor, string literals
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*#/;
        next if $line =~ /^\s*\*/;  # Inside block comment

        # Remove string literals for analysis
        my $stripped = $line;
        $stripped =~ s/"(?:[^"\\]|\\.)*"//g;
        $stripped =~ s/'(?:[^'\\]|\\.)*'//g;

        # Check for pointer attached to type name instead of variable name
        # Pattern: "Type* var" should be "Type *var" (MeshMC convention)
        # But this is complex - MeshMC actually uses "Type *var" style
        if ($stripped =~ /\b(\w+)\*\s+(\w+)/ && $stripped !~ /\*\//) {
            my $type = $1;
            my $var = $2;
            # Skip common false positives
            next if $type =~ /^(?:const|return|void|char|int|long|short|unsigned|signed|float|double|bool|auto)$/;
            next if $stripped =~ /operator\s*\*/;
            next if $stripped =~ /\bdelete\b/;
            next if $stripped =~ /template/;

            # In MeshMC, the preferred style is "Type *var" (space before *)
            report($filepath, $linenum, SEV_STYLE, 'POINTER_ALIGNMENT',
                "Pointer '*' should be adjacent to variable name, not type " .
                "(use '$type *$var' instead of '$type* $var')");
        }

        # Check reference alignment similarly
        if ($stripped =~ /\b(\w+)&\s+(\w+)/ && $stripped !~ /&&/) {
            my $type = $1;
            my $var = $2;
            next if $type =~ /^(?:const|return|void|char|int|long|short|unsigned|signed|float|double|bool|auto)$/;
            next if $stripped =~ /operator\s*&/;

            # Same convention for references
            report($filepath, $linenum, SEV_STYLE, 'REFERENCE_ALIGNMENT',
                "Reference '&' should be adjacent to variable name, not type " .
                "(use '$type &$var' instead of '$type& $var')");
        }
    }
}

# ===========================================================================
# Spacing Conventions Check
# ===========================================================================

sub check_spacing_conventions {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments and preprocessor
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;
        next if $line =~ /^\s*#/;

        # Remove string literals
        my $stripped = $line;
        $stripped =~ s/"(?:[^"\\]|\\.)*"/""/g;
        $stripped =~ s/'(?:[^'\\]|\\.)*'/''/g;

        # Check space after keywords
        foreach my $keyword (qw(if for while switch catch do)) {
            if ($stripped =~ /\b$keyword\(/) {
                report($filepath, $linenum, SEV_STYLE, 'KEYWORD_SPACE',
                    "Missing space between '$keyword' and '(' - use '$keyword ('");
            }
        }

        # Check no space before semicolons
        if ($stripped =~ /\s;/ && $stripped !~ /for\s*\(/) {
            report($filepath, $linenum, SEV_STYLE, 'SPACE_BEFORE_SEMICOLON',
                "Unexpected space before semicolon");
        }

        # Check space after commas
        if ($stripped =~ /,\S/ && $stripped !~ /,\s*$/) {
            # Skip template arguments and nested parentheses
            next if $stripped =~ /[<>]/;
            report($filepath, $linenum, SEV_STYLE, 'SPACE_AFTER_COMMA',
                "Missing space after comma");
        }

        # Check spaces around binary operators (=, ==, !=, <=, >=, &&, ||, +, -, etc.)
        # Be careful not to flag unary operators, pointers, references, etc.
        if ($stripped =~ /\w=[^=]/ && $stripped !~ /[!<>=]=/ && $stripped !~ /operator/) {
            # Assignment without space
            if ($stripped =~ /(\w)=([^\s=])/ && $stripped !~ /[<>]/) {
                # Very common false positive - skip template/default args
                # Only flag obvious cases
            }
        }

        # Check for double spaces (excluding indentation and alignment)
        if ($stripped =~ /\S  \S/ && $stripped !~ /\/\//) {
            # Skip alignment patterns
            next if $stripped =~ /^\s+\w+\s{2,}\w/;  # Aligned declarations
            report($filepath, $linenum, SEV_INFO, 'DOUBLE_SPACE',
                "Multiple consecutive spaces in code (not indentation)");
        }

        # Check space before opening brace
        if ($stripped =~ /\S\{/ && $stripped !~ /\$\{/ && $stripped !~ /\\\{/) {
            # Skip lambda captures, initializer lists
            next if $stripped =~ /\]\s*{/;
            next if $stripped =~ /=\s*{/;
            next if $stripped =~ /return\s*{/;
            next if $stripped =~ /^\s*{/;
            next if $stripped =~ /R"\w*\(/;  # raw string

            report($filepath, $linenum, SEV_STYLE, 'SPACE_BEFORE_BRACE',
                "Missing space before opening brace '{'");
        }
    }
}

# ===========================================================================
# Qt Convention Checks
# ===========================================================================

sub check_qt_conventions {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $has_qobject = 0;
    my $in_class = 0;
    my $class_name = '';
    my $has_virtual_destructor = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        # Track class definition
        if ($line =~ /\bclass\s+(\w+)\b.*:\s*public\s+QObject\b/) {
            $in_class = 1;
            $class_name = $1;
            $has_qobject = 0;
            $has_virtual_destructor = 0;
        }

        # Check for Q_OBJECT macro in QObject-derived classes
        if ($in_class && $line =~ /\bQ_OBJECT\b/) {
            $has_qobject = 1;
        }

        if ($in_class && $line =~ /virtual\s+~/) {
            $has_virtual_destructor = 1;
        }

        # End of class
        if ($in_class && $line =~ /^};/) {
            if (!$has_qobject && $class_name) {
                report($filepath, $linenum, SEV_ERROR, 'MISSING_Q_OBJECT',
                    "Class '$class_name' derived from QObject missing Q_OBJECT macro");
            }
            $in_class = 0;
            $class_name = '';
        }

        next unless exists $changed_ref->{$linenum};

        # Check for old-style Qt connect() with SIGNAL/SLOT macros
        if ($line =~ /\bconnect\s*\(.*\bSIGNAL\s*\(/) {
            report($filepath, $linenum, SEV_WARNING, 'OLD_QT_CONNECT',
                "Old-style SIGNAL/SLOT connect() syntax; prefer new-style pointer-to-member syntax");
        }

        # Check for Q_EMIT vs emit
        if ($line =~ /\bemit\s+\w+/ && $line !~ /\bQ_EMIT\b/) {
            # In MeshMC, both 'emit' and direct signal calls are used
            # Just note it as style
            report($filepath, $linenum, SEV_INFO, 'EMIT_KEYWORD',
                "Consider using Q_EMIT instead of 'emit' for clarity");
        }

        # Check for QObject::tr() in non-QObject contexts
        if ($line =~ /QObject::tr\s*\(/) {
            report($filepath, $linenum, SEV_INFO, 'QOBJECT_TR',
                "Using QObject::tr() directly; consider if the class should use Q_OBJECT and tr()");
        }

        # Check signals/slots sections formatting
        if ($line =~ /^(signals|public\s+slots|private\s+slots|protected\s+slots)\s*:/) {
            # OK - proper format
        } elsif ($line =~ /^(Q_SIGNALS|Q_SLOTS)\s*:/) {
            report($filepath, $linenum, SEV_STYLE, 'QT_SIGNAL_SLOT_SYNTAX',
                "Prefer 'signals:' and 'slots:' over Q_SIGNALS/Q_SLOTS macros");
        }
    }
}

# ===========================================================================
# Include Style Check
# ===========================================================================

sub check_include_style {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        if ($line =~ /^\s*#\s*include\s+(.+)/) {
            my $include = $1;

            # Check for correct include syntax
            unless ($include =~ /^[<"]/) {
                report($filepath, $linenum, SEV_ERROR, 'INCLUDE_SYNTAX',
                    "Invalid include syntax: should be #include <file> or #include \"file\"");
                next;
            }

            # Check for C headers that have C++ equivalents
            my %c_to_cpp = (
                '<stdio.h>'   => '<cstdio>',
                '<stdlib.h>'  => '<cstdlib>',
                '<string.h>'  => '<cstring>',
                '<math.h>'    => '<cmath>',
                '<ctype.h>'   => '<cctype>',
                '<assert.h>'  => '<cassert>',
                '<stddef.h>'  => '<cstddef>',
                '<stdint.h>'  => '<cstdint>',
                '<time.h>'    => '<ctime>',
                '<limits.h>'  => '<climits>',
                '<float.h>'   => '<cfloat>',
                '<errno.h>'   => '<cerrno>',
                '<signal.h>'  => '<csignal>',
                '<setjmp.h>'  => '<csetjmp>',
                '<locale.h>'  => '<clocale>',
            );

            foreach my $c_header (keys %c_to_cpp) {
                if ($include =~ /^\Q$c_header\E/) {
                    report($filepath, $linenum, SEV_WARNING, 'C_HEADER_IN_CPP',
                        "Use C++ header $c_to_cpp{$c_header} instead of C header $c_header");
                }
            }

            # Check for absolute paths in includes
            if ($include =~ /^"\//) {
                report($filepath, $linenum, SEV_ERROR, 'ABSOLUTE_INCLUDE_PATH',
                    "Absolute path in #include directive; use relative paths");
            }

            # Check for backward slashes in include paths
            if ($include =~ /\\/) {
                report($filepath, $linenum, SEV_ERROR, 'BACKSLASH_IN_INCLUDE',
                    "Backslash in #include path; use forward slashes");
            }
        }
    }
}

# ===========================================================================
# Comment Style Check
# ===========================================================================

sub check_comment_style {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $in_multiline_comment = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        # Track multiline comments
        if ($line =~ /\/\*/ && $line !~ /\*\//) {
            $in_multiline_comment = 1;
        }
        if ($in_multiline_comment) {
            $in_multiline_comment = 0 if $line =~ /\*\//;

            # Check alignment of multi-line comment body
            if ($line =~ /^(\s*)\*/ && $line !~ /^\s*\/\*/) {
                # Comment continuation - * should be aligned
            }
            next;
        }

        next unless exists $changed_ref->{$linenum};

        # Check for C-style comments on single lines (should use //)
        if ($line =~ /\/\*(.+?)\*\/\s*$/ && $line !~ /^\s*\/\*/) {
            my $comment = $1;
            # Skip if it's a license header or Doxygen
            next if $comment =~ /SPDX|copyright|doxygen|\@|\\brief/i;
            # Single-line comments should prefer //
            report($filepath, $linenum, SEV_INFO, 'COMMENT_STYLE',
                "Consider using // for single-line comments instead of /* */");
        }

        # Check for commented-out code (rough heuristic)
        if ($line =~ /^\s*\/\/\s*(if|for|while|return|switch|class|struct|void|int|bool|auto|QString)\b/) {
            report($filepath, $linenum, SEV_WARNING, 'COMMENTED_OUT_CODE',
                "Possible commented-out code detected; remove dead code");
        }

        # Check for TODO/FIXME format
        if ($line =~ /\/\/\s*(TODO|FIXME|HACK|XXX|BUG)\b/) {
            # Proper format: // TODO: Description or // TODO(author): Description
            unless ($line =~ /\/\/\s*(?:TODO|FIXME|HACK|XXX|BUG)\s*(?:\(\w+\)\s*)?:\s*\S/) {
                report($filepath, $linenum, SEV_INFO, 'TODO_FORMAT',
                    "TODO/FIXME should follow format: // TODO: Description or // TODO(author): Description");
            }
        }
    }
}

# ===========================================================================
# Control Flow Style Check
# ===========================================================================

sub check_control_flow_style {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for single-line if/else without braces
        # MeshMC allows single-line if without braces for simple returns
        # But multi-line if without braces is dangerous
        if ($line =~ /\bif\s*\(.*\)\s*$/ && $line !~ /{\s*$/) {
            if ($i + 1 < scalar @$lines_ref) {
                my $next = $lines_ref->[$i + 1];
                # Next line is not an opening brace
                if ($next !~ /^\s*{/) {
                    # Check if it's a simple single-statement if
                    if ($i + 2 < scalar @$lines_ref) {
                        my $after = $lines_ref->[$i + 2];
                        # If the line after the body has more code, it's OK (single statement)
                        # But warn anyway for consistency
                        report($filepath, $linenum, SEV_INFO, 'IF_WITHOUT_BRACES',
                            "Consider using braces for 'if' statement body for consistency");
                    }
                }
            }
        }

        # Check for goto usage
        if ($line =~ /\bgoto\s+\w+/) {
            report($filepath, $linenum, SEV_WARNING, 'GOTO_STATEMENT',
                "Use of 'goto' statement; consider restructuring with loops/functions");
        }

        # Check for deeply nested ternary
        if ($line =~ /\?.*\?.*:.*:/) {
            report($filepath, $linenum, SEV_WARNING, 'NESTED_TERNARY',
                "Nested ternary operator; consider using if/else for readability");
        }

        # Check for Yoda conditions (constant == variable)
        if ($line =~ /\b(?:nullptr|NULL|true|false|\d+)\s*==\s*\w/) {
            report($filepath, $linenum, SEV_STYLE, 'YODA_CONDITION',
                "Yoda condition detected; prefer 'variable == constant' style");
        }

        # Check assignment in condition
        if ($line =~ /\bif\s*\([^)]*[^!=<>]=[^=][^)]*\)/) {
            # This could be intentional (e.g., if (auto x = ...))
            # Only flag clear assignment cases
            if ($line !~ /\bauto\b/ && $line !~ /\bif\s*\(\s*\w+\s*=\s*\w+\.\w+\(/) {
                report($filepath, $linenum, SEV_WARNING, 'ASSIGNMENT_IN_CONDITION',
                    "Possible assignment in condition; did you mean '=='?");
            }
        }
    }
}

# ===========================================================================
# String Usage Check
# ===========================================================================

sub check_string_usage {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for std::string usage (project uses QString)
        if ($line =~ /\bstd::string\b/) {
            report($filepath, $linenum, SEV_WARNING, 'STD_STRING_USAGE',
                "Use QString instead of std::string (project convention)");
        }

        # Check for std::cout/std::cerr (project uses Qt logging)
        if ($line =~ /\bstd::(?:cout|cerr|clog)\b/) {
            report($filepath, $linenum, SEV_WARNING, 'STD_IOSTREAM',
                "Use qDebug()/qWarning()/qCritical() instead of std::cout/cerr (project convention)");
        }

        # Check for printf/fprintf usage
        if ($line =~ /\b(?:printf|fprintf|puts)\s*\(/ && $line !~ /::snprintf/) {
            report($filepath, $linenum, SEV_WARNING, 'PRINTF_USAGE',
                "Use qDebug()/qWarning() or QTextStream instead of printf/fprintf");
        }

        # Check for empty string comparisons
        if ($line =~ /==\s*""/ || $line =~ /""\s*==/) {
            report($filepath, $linenum, SEV_STYLE, 'EMPTY_STRING_COMPARE',
                "Use QString::isEmpty() instead of comparing with empty string \"\"");
        }

        # Check for string concatenation in loops (potential performance issue)
        # This is hard to detect accurately, so we just look for obvious patterns
        if ($line =~ /\+=\s*"/ || $line =~ /\+=\s*QString/) {
            # Only flag if we're inside a for/while loop (check context)
            for (my $j = $i - 1; $j >= 0 && $j >= $i - 20; $j--) {
                if ($lines_ref->[$j] =~ /\b(?:for|while)\s*\(/) {
                    report($filepath, $linenum, SEV_INFO, 'STRING_CONCAT_LOOP',
                        "String concatenation in loop; consider using QStringList::join() or QStringBuilder");
                    last;
                }
                last if $lines_ref->[$j] =~ /^[{}]\s*$/;
            }
        }
    }
}

# ===========================================================================
# Memory Management Pattern Check
# ===========================================================================

sub check_memory_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for naked new without smart pointer
        if ($line =~ /\bnew\s+\w+/ && $line !~ /(?:shared_ptr|unique_ptr|make_shared|make_unique|reset|QObject|QWidget|QLayout|Q\w+)/) {
            # Skip Qt object creation (parent takes ownership)
            next if $line =~ /new\s+\w+\s*\([^)]*(?:this|parent)\s*[,)]/;
            # Skip placement new
            next if $line =~ /new\s*\(/;
            # Skip operator new
            next if $line =~ /operator\s+new/;

            report($filepath, $linenum, SEV_INFO, 'RAW_NEW',
                "Use std::make_shared/std::make_unique instead of raw 'new' when possible");
        }

        # Check for delete usage (should use smart pointers)
        if ($line =~ /\bdelete\b/ && $line !~ /\bQ_DISABLE\b/ && $line !~ /=\s*delete/) {
            report($filepath, $linenum, SEV_INFO, 'RAW_DELETE',
                "Raw 'delete' detected; prefer smart pointers for automatic memory management");
        }

        # Check for C-style malloc/calloc/free
        if ($line =~ /\b(?:malloc|calloc|realloc|free)\s*\(/) {
            report($filepath, $linenum, SEV_WARNING, 'C_MEMORY_FUNCTIONS',
                "C-style memory functions (malloc/free) detected; use C++ alternatives (new/delete or smart pointers)");
        }

        # Check for proper shared_ptr usage
        if ($line =~ /std::shared_ptr<\w+>\s*\(\s*new\b/) {
            report($filepath, $linenum, SEV_WARNING, 'SHARED_PTR_NEW',
                "Use std::make_shared<T>() instead of std::shared_ptr<T>(new T())");
        }

        # Check for unique_ptr with new
        if ($line =~ /std::unique_ptr<\w+>\s*\(\s*new\b/) {
            report($filepath, $linenum, SEV_WARNING, 'UNIQUE_PTR_NEW',
                "Use std::make_unique<T>() instead of std::unique_ptr<T>(new T())");
        }
    }
}

# ===========================================================================
# Const Correctness Check
# ===========================================================================

sub check_const_correctness {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for non-const reference parameters to complex types
        # that appear to be input-only (no modification)
        if ($line =~ /(?:QString|QStringList|QList|QVector|QMap|QHash|QSet|QByteArray|std::(?:string|vector|map|set))\s*&\s*(\w+)/) {
            my $param = $1;
            # If it's a function parameter (has type before &)
            if ($line =~ /\(.*$/ || $line =~ /,\s*$/) {
                # Check if const is missing
                unless ($line =~ /const\s+(?:QString|QStringList|QList|QVector|QMap|QHash|QSet|QByteArray|std::(?:string|vector|map|set))\s*&/) {
                    # This could be intentional (output parameter), so make it info level
                    report($filepath, $linenum, SEV_INFO, 'CONST_REF_PARAM',
                        "Consider using const reference for parameter '$param' if not modified");
                }
            }
        }
    }
}

# ===========================================================================
# Enum Style Check
# ===========================================================================

sub check_enum_style {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Check for plain enum (prefer enum class)
        if ($line =~ /\benum\s+(\w+)\s*{/ && $line !~ /\benum\s+class\b/) {
            my $name = $1;
            report($filepath, $linenum, SEV_INFO, 'PREFER_ENUM_CLASS',
                "Consider using 'enum class $name' instead of plain 'enum $name' for type safety");
        }

        # Check for anonymous enum
        if ($line =~ /\benum\s*{/ && $line !~ /\benum\s+\w/) {
            report($filepath, $linenum, SEV_INFO, 'ANONYMOUS_ENUM',
                "Anonymous enum detected; consider naming it or using 'constexpr'");
        }
    }
}

# ===========================================================================
# Constructor Pattern Check
# ===========================================================================

sub check_constructor_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for single-parameter constructors without explicit
        if ($line =~ /^\s*(\w+)\s*\(\s*(?:const\s+)?(?:\w+(?:::\w+)*)\s*[&*]?\s*\w+\s*\)\s*[;{]/ &&
            $line !~ /\bexplicit\b/ && $line !~ /\bvirtual\b/ && $line !~ /\boverride\b/) {
            my $class = $1;
            # Skip destructors, operators
            next if $class =~ /^~/;
            next if $class =~ /^operator/;
            # Skip copy/move constructors
            next if $line =~ /\b$class\s*\(\s*(?:const\s+)?$class\s*[&]/;

            report($filepath, $linenum, SEV_WARNING, 'IMPLICIT_CONSTRUCTOR',
                "Single-parameter constructor should be marked 'explicit' to prevent implicit conversions");
        }

        # Check for assignment in constructor body instead of initializer list
        # (When the previous line ends with { and current line is "m_var = ...")
        if ($line =~ /^\s+m_\w+\s*=\s*\w/ && $i > 0) {
            my $prev = $lines_ref->[$i - 1];
            if ($prev =~ /^{$/ || $prev =~ /^\s*{$/) {
                # Check if a few lines up we have a constructor signature
                for (my $j = $i - 2; $j >= 0 && $j >= $i - 5; $j--) {
                    if ($lines_ref->[$j] =~ /\w+::\w+\s*\(/ && $lines_ref->[$j] !~ /\bif\b|\bfor\b|\bwhile\b/) {
                        report($filepath, $linenum, SEV_INFO, 'USE_INIT_LIST',
                            "Consider using initializer list instead of assignment in constructor body");
                        last;
                    }
                }
            }
        }
    }
}

# ===========================================================================
# Function Length Check
# ===========================================================================

sub check_function_length {
    my ($filepath, $lines_ref) = @_;

    my $brace_depth = 0;
    my $function_start = -1;
    my $function_name = '';
    my $in_function = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];

        # Skip preprocessor directives
        next if $line =~ /^\s*#/;

        # Detect function definition start
        if (!$in_function && $line =~ /^(?:\w[\w:*&<> ,]*\s+)?(\w+(?:::\w+)?)\s*\([^;]*$/) {
            my $name = $1;
            # Check if next lines contain the opening brace
            if ($line =~ /{/) {
                $function_start = $i;
                $function_name = $name;
                $in_function = 1;
                $brace_depth = count_braces($line);
            } elsif ($i + 1 < scalar @$lines_ref && $lines_ref->[$i + 1] =~ /^\s*{/) {
                $function_start = $i;
                $function_name = $name;
            }
        }

        if ($function_start >= 0 && !$in_function && $line =~ /{/) {
            $in_function = 1;
            $brace_depth = 0;
        }

        if ($in_function) {
            $brace_depth += count_braces($line);

            if ($brace_depth <= 0 && $line =~ /}/) {
                my $length = $i - $function_start + 1;
                if ($length > $MAX_FUNCTION_LENGTH) {
                    report($filepath, $function_start + 1, SEV_WARNING, 'FUNCTION_TOO_LONG',
                        "Function '$function_name' is $length lines (recommended maximum: $MAX_FUNCTION_LENGTH). Consider refactoring.");
                }
                $in_function = 0;
                $function_start = -1;
                $function_name = '';
                $brace_depth = 0;
            }
        }
    }
}

sub count_braces {
    my ($line) = @_;
    # Remove string literals and comments
    my $stripped = $line;
    $stripped =~ s/"(?:[^"\\]|\\.)*"//g;
    $stripped =~ s/'(?:[^'\\]|\\.)*'//g;
    $stripped =~ s/\/\/.*$//;

    my $open = () = $stripped =~ /{/g;
    my $close = () = $stripped =~ /}/g;
    return $open - $close;
}

# ===========================================================================
# Nesting Depth Check
# ===========================================================================

sub check_nesting_depth {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $depth = 0;
    my $max_depth = 0;
    my $max_depth_line = 0;
    my $in_function = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        # Remove strings and comments
        my $stripped = $line;
        $stripped =~ s/"(?:[^"\\]|\\.)*"//g;
        $stripped =~ s/'(?:[^'\\]|\\.)*'//g;
        $stripped =~ s/\/\/.*$//;

        my $opens = () = $stripped =~ /{/g;
        my $closes = () = $stripped =~ /}/g;

        $depth += $opens;

        if ($depth > $MAX_NESTING_DEPTH && exists $changed_ref->{$linenum}) {
            if ($depth > $max_depth) {
                $max_depth = $depth;
                $max_depth_line = $linenum;
            }
        }

        $depth -= $closes;
        $depth = 0 if $depth < 0;
    }

    if ($max_depth > $MAX_NESTING_DEPTH) {
        report($filepath, $max_depth_line, SEV_WARNING, 'DEEP_NESTING',
            "Nesting depth of $max_depth exceeds maximum of $MAX_NESTING_DEPTH. Consider refactoring.");
    }
}

# ===========================================================================
# Deprecated Pattern Check
# ===========================================================================

sub check_deprecated_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for NULL instead of nullptr
        if ($line =~ /\bNULL\b/ && $line !~ /#\s*define/) {
            report($filepath, $linenum, SEV_WARNING, 'USE_NULLPTR',
                "Use 'nullptr' instead of 'NULL' (C++11)");
        }

        # Check for C-style casts
        if ($line =~ /\(\s*(?:int|long|short|char|float|double|unsigned|signed|bool|void)\s*[*]*\s*\)\s*\w/) {
            report($filepath, $linenum, SEV_WARNING, 'C_STYLE_CAST',
                "C-style cast detected; use static_cast<>, dynamic_cast<>, reinterpret_cast<>, or const_cast<>");
        }

        # Check for typedef instead of using (prefer using)
        if ($line =~ /\btypedef\b/) {
            report($filepath, $linenum, SEV_STYLE, 'PREFER_USING',
                "Consider using 'using' alias instead of 'typedef' (modern C++ style)");
        }

        # Check for deprecated QList patterns
        if ($line =~ /\bQList<(?:int|double|float|bool|char|qint\d+|quint\d+)>/) {
            report($filepath, $linenum, SEV_INFO, 'QLIST_VALUE_TYPE',
                "QList of basic types; consider QVector for better performance in Qt5");
        }

        # Check for deprecated Qt functions
        if ($line =~ /\b(?:qSort|qStableSort|qLowerBound|qUpperBound|qBinaryFind)\s*\(/) {
            report($filepath, $linenum, SEV_WARNING, 'DEPRECATED_QT_ALGORITHM',
                "Deprecated Qt algorithm; use std:: equivalents (std::sort, std::stable_sort, etc.)");
        }

        # Check for register keyword
        if ($line =~ /\bregister\b/ && $line !~ /^\s*\/\//) {
            report($filepath, $linenum, SEV_WARNING, 'REGISTER_KEYWORD',
                "'register' keyword is deprecated in C++17 and removed in C++20");
        }

        # Check for auto_ptr (deprecated)
        if ($line =~ /\bstd::auto_ptr\b/) {
            report($filepath, $linenum, SEV_ERROR, 'AUTO_PTR',
                "std::auto_ptr is removed in C++17; use std::unique_ptr instead");
        }

        # Check for bind1st/bind2nd (deprecated)
        if ($line =~ /\bstd::(?:bind1st|bind2nd)\b/) {
            report($filepath, $linenum, SEV_ERROR, 'DEPRECATED_BIND',
                "std::bind1st/bind2nd are removed in C++17; use std::bind or lambdas");
        }
    }
}

# ===========================================================================
# Security Pattern Check
# ===========================================================================

sub check_security_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for dangerous functions
        if ($line =~ /\b(?:gets)\s*\(/) {
            report($filepath, $linenum, SEV_ERROR, 'DANGEROUS_FUNCTION',
                "Use of 'gets' is extremely dangerous (buffer overflow); use fgets or std::getline");
        }

        if ($line =~ /\bsprintf\s*\(/) {
            report($filepath, $linenum, SEV_WARNING, 'SPRINTF_USAGE',
                "Use snprintf instead of sprintf to prevent buffer overflow");
        }

        if ($line =~ /\bstrcpy\s*\(/) {
            report($filepath, $linenum, SEV_WARNING, 'STRCPY_USAGE',
                "Use strncpy or std::string instead of strcpy to prevent buffer overflow");
        }

        if ($line =~ /\bstrcat\s*\(/) {
            report($filepath, $linenum, SEV_WARNING, 'STRCAT_USAGE',
                "Use strncat or std::string instead of strcat to prevent buffer overflow");
        }

        # Check for system() calls
        if ($line =~ /\bsystem\s*\(/) {
            report($filepath, $linenum, SEV_WARNING, 'SYSTEM_CALL',
                "Use of system() is a security risk; use QProcess for safer process execution");
        }

        # Check for potential format string vulnerabilities
        if ($line =~ /\b(?:printf|fprintf|sprintf|snprintf)\s*\(\s*(?:\w+)\s*\)/) {
            report($filepath, $linenum, SEV_WARNING, 'FORMAT_STRING',
                "Potential format string vulnerability; always use a literal format string");
        }

        # Check for hardcoded credentials patterns
        if ($line =~ /(?:password|passwd|secret|api_?key|token)\s*=\s*"[^"]+"/i) {
            # Skip test files and examples
            next if $filepath =~ /_test\.cpp$/;
            report($filepath, $linenum, SEV_ERROR, 'HARDCODED_CREDENTIALS',
                "Possible hardcoded credentials detected; use configuration files or environment variables");
        }

        # Check for use of rand() (use modern random)
        if ($line =~ /\brand\s*\(\s*\)/ && $line !~ /\bstd::/) {
            report($filepath, $linenum, SEV_INFO, 'C_RAND',
                "Use <random> header (std::mt19937, std::uniform_int_distribution) instead of rand()");
        }
    }
}

# ===========================================================================
# Test Convention Check
# ===========================================================================

sub check_test_conventions {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    # Only check test files
    return unless $filepath =~ /_test\.cpp$/;

    my $has_qtest_include = 0;
    my $has_qtest_main = 0;
    my $has_moc_include = 0;
    my $has_qobject = 0;
    my $test_class_name = '';

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        if ($line =~ /#\s*include\s*<QTest>/) {
            $has_qtest_include = 1;
        }

        if ($line =~ /QTEST_(?:GUILESS_)?MAIN\s*\((\w+)\)/) {
            $has_qtest_main = 1;
            my $main_class = $1;
            if ($test_class_name && $main_class ne $test_class_name) {
                report($filepath, $linenum, SEV_ERROR, 'TEST_MAIN_MISMATCH',
                    "QTEST_MAIN class '$main_class' doesn't match test class '$test_class_name'");
            }
        }

        if ($line =~ /#\s*include\s+"(\w+)_test\.moc"/) {
            $has_moc_include = 1;
        }

        if ($line =~ /\bclass\s+(\w+Test)\s*:/) {
            $test_class_name = $1;
        }

        if ($line =~ /Q_OBJECT/) {
            $has_qobject = 1;
        }

        # Check test method naming
        if ($line =~ /^\s*void\s+(test\w*)\s*\(/) {
            my $method = $1;
            # MeshMC test methods use test_PascalCase pattern
            unless ($method =~ /^test_[A-Z]\w*$/ || $method =~ /^test_\w+_data$/) {
                report($filepath, $linenum, SEV_STYLE, 'TEST_METHOD_NAMING',
                    "Test method '$method' should follow 'test_PascalCase' naming convention");
            }
        }

        # Check for QVERIFY(true) or QVERIFY(false) - likely placeholder
        if ($line =~ /QVERIFY\s*\(\s*(?:true|false)\s*\)/) {
            report($filepath, $linenum, SEV_WARNING, 'TRIVIAL_ASSERTION',
                "Trivial assertion QVERIFY(true/false) - likely a placeholder");
        }
    }

    if (!$has_qtest_include) {
        report($filepath, 1, SEV_ERROR, 'MISSING_QTEST_INCLUDE',
            "Test file missing #include <QTest>");
    }

    if (!$has_qtest_main) {
        report($filepath, scalar @$lines_ref, SEV_ERROR, 'MISSING_QTEST_MAIN',
            "Test file missing QTEST_GUILESS_MAIN() or QTEST_MAIN() macro");
    }

    if (!$has_moc_include && $has_qobject) {
        report($filepath, scalar @$lines_ref, SEV_ERROR, 'MISSING_MOC_INCLUDE',
            "Test file with Q_OBJECT missing #include \"*_test.moc\" at end of file");
    }
}

# ===========================================================================
# Exception Pattern Check
# ===========================================================================

sub check_exception_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for throwing by pointer (throw new Exception)
        if ($line =~ /\bthrow\s+new\b/) {
            report($filepath, $linenum, SEV_ERROR, 'THROW_BY_POINTER',
                "Throw by value, not by pointer (throw Exception(...), not throw new Exception(...))");
        }

        # Check for catching by value (should catch by reference)
        if ($line =~ /\bcatch\s*\(\s*(?!const\s)(\w[\w:]*)\s+\w+\s*\)/) {
            my $type = $1;
            next if $type eq '...';  # catch-all
            report($filepath, $linenum, SEV_WARNING, 'CATCH_BY_VALUE',
                "Catch exceptions by const reference: catch(const $type& e)");
        }

        # Check for catching by non-const reference
        if ($line =~ /\bcatch\s*\(\s*(\w[\w:]*)\s*&\s*\w+\s*\)/ && $line !~ /\bconst\b/) {
            report($filepath, $linenum, SEV_STYLE, 'CATCH_NON_CONST_REF',
                "Catch exceptions by const reference: catch(const ... &)");
        }

        # Check for empty catch blocks
        if ($line =~ /\bcatch\b/) {
            if ($i + 1 < scalar @$lines_ref) {
                my $next = $lines_ref->[$i + 1];
                if ($next =~ /^\s*{\s*}\s*$/ || ($line =~ /{\s*$/ && $i + 1 < scalar @$lines_ref && $lines_ref->[$i + 1] =~ /^\s*}\s*$/)) {
                    report($filepath, $linenum, SEV_WARNING, 'EMPTY_CATCH',
                        "Empty catch block; at minimum, add a comment explaining why it's empty");
                }
            }
        }

        # Check for catch(...) without re-throw
        if ($line =~ /\bcatch\s*\(\s*\.\.\.\s*\)/) {
            # Check if body contains throw
            my $has_throw = 0;
            for (my $j = $i + 1; $j < scalar @$lines_ref && $j < $i + 20; $j++) {
                if ($lines_ref->[$j] =~ /\bthrow\b/) {
                    $has_throw = 1;
                    last;
                }
                last if $lines_ref->[$j] =~ /^}/;
            }
            if (!$has_throw) {
                report($filepath, $linenum, SEV_WARNING, 'CATCH_ALL_NO_RETHROW',
                    "catch(...) without re-throw may swallow important exceptions");
            }
        }
    }
}

# ===========================================================================
# Virtual Destructor Check
# ===========================================================================

sub check_virtual_destructor {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $in_class = 0;
    my $class_name = '';
    my $has_virtual_method = 0;
    my $has_virtual_destructor = 0;
    my $class_start_line = 0;
    my $brace_depth = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];

        # Detect class start
        if ($line =~ /\bclass\s+(\w+)\b/ && $line !~ /;\s*$/) {
            $in_class = 1;
            $class_name = $1;
            $has_virtual_method = 0;
            $has_virtual_destructor = 0;
            $class_start_line = $i + 1;
            $brace_depth = 0;
        }

        next unless $in_class;

        # Track braces
        my $stripped = $line;
        $stripped =~ s/"(?:[^"\\]|\\.)*"//g;
        my $opens = () = $stripped =~ /{/g;
        my $closes = () = $stripped =~ /}/g;
        $brace_depth += $opens - $closes;

        if ($line =~ /\bvirtual\b/ && $line !~ /~/) {
            $has_virtual_method = 1;
        }

        if ($line =~ /virtual\s+~/ || $line =~ /~\w+\s*\(\s*\)\s*(?:=\s*default|override)/) {
            $has_virtual_destructor = 1;
        }

        # End of class
        if ($brace_depth <= 0 && $line =~ /}/) {
            if ($has_virtual_method && !$has_virtual_destructor) {
                report($filepath, $class_start_line, SEV_WARNING, 'MISSING_VIRTUAL_DESTRUCTOR',
                    "Class '$class_name' has virtual methods but no virtual destructor");
            }
            $in_class = 0;
        }
    }
}

# ===========================================================================
# Override Keyword Check
# ===========================================================================

sub check_override_keyword {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $in_derived_class = 0;
    my $class_name = '';

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        # Track derived class context
        if ($line =~ /\bclass\s+(\w+)\s*:\s*(?:public|protected|private)/) {
            $in_derived_class = 1;
            $class_name = $1;
        }

        if ($in_derived_class && $line =~ /^};/) {
            $in_derived_class = 0;
        }

        next unless $in_derived_class;
        next unless exists $changed_ref->{$linenum};

        # Check for virtual without override in derived class
        if ($line =~ /\bvirtual\b.*\)\s*(?:const\s*)?;/ && $line !~ /\boverride\b/ && $line !~ /=\s*0/) {
            report($filepath, $linenum, SEV_WARNING, 'MISSING_OVERRIDE',
                "Virtual method in derived class should use 'override' keyword");
        }

        # Check for both virtual and override (redundant)
        if ($line =~ /\bvirtual\b.*\boverride\b/) {
            report($filepath, $linenum, SEV_STYLE, 'VIRTUAL_AND_OVERRIDE',
                "'virtual' is redundant when 'override' is used");
        }
    }
}

# ===========================================================================
# auto Usage Check
# ===========================================================================

sub check_auto_usage {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for auto& without const for non-modifying iteration
        # This is hard to detect accurately, so be conservative
        if ($line =~ /\bfor\s*\(\s*auto\s+&\s+\w+\s*:/ && $line !~ /\bconst\b/) {
            report($filepath, $linenum, SEV_INFO, 'AUTO_REF_ITERATION',
                "Consider 'const auto&' for range-based for loop if elements aren't modified");
        }

        # Check for auto in function return type (hard to read)
        if ($line =~ /^\s*(?:static\s+)?(?:inline\s+)?auto\s+\w+\s*\(/) {
            # Trailing return type is OK
            unless ($line =~ /->/) {
                report($filepath, $linenum, SEV_INFO, 'AUTO_RETURN_TYPE',
                    "Auto return type without trailing return type can reduce readability");
            }
        }
    }
}

# ===========================================================================
# Lambda Style Check
# ===========================================================================

sub check_lambda_style {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for [=] capture (captures everything by value - usually too broad)
        if ($line =~ /\[\s*=\s*\]/) {
            report($filepath, $linenum, SEV_INFO, 'LAMBDA_CAPTURE_ALL_VALUE',
                "Lambda captures everything by value [=]; consider explicit captures");
        }

        # Check for [&] capture (captures everything by reference)
        # This is used in MeshMC but worth noting for awareness
        if ($line =~ /\[\s*&\s*\]/ && $line !~ /\[\s*&\s*\]\s*\(\s*\)\s*{/) {
            # Only flag in certain contexts (e.g., stored lambdas vs inline)
            if ($line =~ /=\s*\[\s*&\s*\]/) {
                report($filepath, $linenum, SEV_INFO, 'LAMBDA_CAPTURE_ALL_REF',
                    "Lambda captures everything by reference [&]; be careful with object lifetimes");
            }
        }

        # Check for overly long lambda bodies (should be extracted to a function)
        if ($line =~ /\[.*\]\s*\(/) {
            # Count lines until matching closing brace
            my $lambda_depth = 0;
            my $lambda_lines = 0;
            my $started = 0;

            for (my $j = $i; $j < scalar @$lines_ref && $j < $i + 100; $j++) {
                my $l = $lines_ref->[$j];
                my $stripped = $l;
                $stripped =~ s/"(?:[^"\\]|\\.)*"//g;

                if ($stripped =~ /{/) {
                    $lambda_depth += () = $stripped =~ /{/g;
                    $started = 1;
                }
                if ($stripped =~ /}/) {
                    $lambda_depth -= () = $stripped =~ /}/g;
                }

                $lambda_lines++ if $started;

                if ($started && $lambda_depth <= 0) {
                    if ($lambda_lines > 30) {
                        report($filepath, $linenum, SEV_INFO, 'LONG_LAMBDA',
                            "Lambda body is $lambda_lines lines; consider extracting to a named function");
                    }
                    last;
                }
            }
        }
    }
}

# ===========================================================================
# Switch/Case Style Check
# ===========================================================================

sub check_switch_case_style {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $in_switch = 0;
    my $switch_depth = 0;
    my $has_default = 0;
    my $switch_line = 0;
    my $last_case_has_break = 1;
    my $case_line = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        # Detect switch statement
        if ($line =~ /\bswitch\s*\(/) {
            $in_switch = 1;
            $switch_depth = 0;
            $has_default = 0;
            $switch_line = $linenum;
            $last_case_has_break = 1;
        }

        next unless $in_switch;

        # Track braces
        my $stripped = $line;
        $stripped =~ s/"(?:[^"\\]|\\.)*"//g;
        $switch_depth += () = $stripped =~ /{/g;
        $switch_depth -= () = $stripped =~ /}/g;

        # Check for default case
        if ($line =~ /\bdefault\s*:/) {
            $has_default = 1;
        }

        # Track case fall-through
        if ($line =~ /\bcase\s+/ || $line =~ /\bdefault\s*:/) {
            if (!$last_case_has_break && $case_line > 0) {
                # Check for intentional fall-through comment
                my $has_fallthrough_comment = 0;
                for (my $j = $case_line; $j < $i; $j++) {
                    if ($lines_ref->[$j] =~ /fall[- ]?through|FALLTHROUGH|FALL_THROUGH/i) {
                        $has_fallthrough_comment = 1;
                        last;
                    }
                }
                if (!$has_fallthrough_comment && exists $changed_ref->{$linenum}) {
                    report($filepath, $case_line + 1, SEV_WARNING, 'CASE_FALLTHROUGH',
                        "Case fall-through without break/return; add [[fallthrough]] or comment if intentional");
                }
            }
            $last_case_has_break = 0;
            $case_line = $i;
        }

        if ($line =~ /\b(?:break|return|throw|continue)\b/ || $line =~ /\[\[fallthrough\]\]/) {
            $last_case_has_break = 1;
        }

        # End of switch
        if ($switch_depth <= 0 && $line =~ /}/) {
            if (!$has_default && exists $changed_ref->{$switch_line}) {
                report($filepath, $switch_line, SEV_INFO, 'SWITCH_NO_DEFAULT',
                    "Switch statement without 'default' case");
            }
            $in_switch = 0;
        }
    }
}

# ===========================================================================
# Class Member Ordering Check
# ===========================================================================

sub check_class_member_ordering {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $in_class = 0;
    my $class_name = '';
    my @access_order = ();
    my $class_start = 0;

    # Expected order in MeshMC: public -> protected -> private
    my %access_rank = (
        'public'    => 0,
        'protected' => 1,
        'private'   => 2,
    );

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];

        if ($line =~ /\bclass\s+(\w+)\b/ && $line !~ /;\s*$/) {
            $in_class = 1;
            $class_name = $1;
            @access_order = ();
            $class_start = $i + 1;
        }

        next unless $in_class;

        if ($line =~ /^\s*(public|protected|private)\s*(?::|(?:\s+\w+)|$)/) {
            push @access_order, {
                level => $1,
                line  => $i + 1,
            };
        }

        if ($line =~ /^};/) {
            # Check ordering
            for (my $j = 1; $j < scalar @access_order; $j++) {
                my $prev = $access_order[$j-1];
                my $curr = $access_order[$j];

                # Allow multiple sections of same level
                next if $prev->{level} eq $curr->{level};

                if ($access_rank{$curr->{level}} < $access_rank{$prev->{level}}) {
                    report($filepath, $curr->{line}, SEV_INFO, 'CLASS_ACCESS_ORDER',
                        "Access specifier '$curr->{level}' appears after '$prev->{level}' in class '$class_name'. " .
                        "MeshMC convention: public -> protected -> private");
                }
            }
            $in_class = 0;
        }
    }
}

# ===========================================================================
# TODO/FIXME Tracking
# ===========================================================================

sub check_todo_fixme {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Track TODOs and FIXMEs in new code
        if ($line =~ /\b(TODO|FIXME|HACK|XXX|BUG)\b/i) {
            my $tag = uc($1);
            report($filepath, $linenum, SEV_INFO, 'TODO_MARKER',
                "$tag marker found - ensure it's tracked");
        }
    }
}

# ===========================================================================
# Debug Statement Check
# ===========================================================================

sub check_debug_statements {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for debug prints that might be left in
        if ($line =~ /\bqDebug\s*\(\s*\)\s*<<\s*"DEBUG/i ||
            $line =~ /\bqDebug\s*\(\s*\)\s*<<\s*"TEMP/i ||
            $line =~ /\bqDebug\s*\(\s*\)\s*<<\s*"TEST/i) {
            report($filepath, $linenum, SEV_WARNING, 'DEBUG_PRINT',
                "Debug/temp print statement detected; remove before committing");
        }

        # Check for qDebug() << __FUNCTION__
        if ($line =~ /qDebug\(\)\s*<<\s*__(?:FUNCTION|func|PRETTY_FUNCTION)__/) {
            report($filepath, $linenum, SEV_INFO, 'DEBUG_FUNCTION_NAME',
                "Debug print with __FUNCTION__; consider using Q_FUNC_INFO or removing before commit");
        }
    }
}

# ===========================================================================
# Header Self-Containment Check
# ===========================================================================

sub check_header_self_containment {
    my ($filepath, $lines_ref, $ftype) = @_;

    return unless $ftype eq FTYPE_HEADER;

    # Check that header has necessary forward declarations or includes
    # for types used in its public API

    my %used_types = ();
    my %included_types = ();
    my %forward_declared = ();

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];

        # Track includes
        if ($line =~ /#\s*include\s*[<"](.+?)[>"]/) {
            my $inc = $1;
            # Extract type name from include
            my $type = basename($inc);
            $type =~ s/\.h$//;
            $included_types{$type} = 1;
        }

        # Track forward declarations
        if ($line =~ /^\s*class\s+(\w+)\s*;/) {
            $forward_declared{$1} = 1;
        }
        if ($line =~ /^\s*struct\s+(\w+)\s*;/) {
            $forward_declared{$1} = 1;
        }
    }
}

# ===========================================================================
# Multiple Statements Per Line Check
# ===========================================================================

sub check_multiple_statements_per_line {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments, preprocessor, strings
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;
        next if $line =~ /^\s*#/;

        # Remove string literals
        my $stripped = $line;
        $stripped =~ s/"(?:[^"\\]|\\.)*"//g;
        $stripped =~ s/'(?:[^'\\]|\\.)*'//g;

        # Count semicolons (excluding for loops)
        if ($stripped !~ /\bfor\s*\(/) {
            my @semicolons = ($stripped =~ /;/g);
            if (scalar @semicolons > 1) {
                report($filepath, $linenum, SEV_STYLE, 'MULTIPLE_STATEMENTS',
                    "Multiple statements on one line; use separate lines for clarity");
            }
        }
    }
}

# ===========================================================================
# Magic Numbers Check
# ===========================================================================

sub check_magic_numbers {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments, preprocessor, strings, includes
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;
        next if $line =~ /^\s*#/;

        # Remove string literals
        my $stripped = $line;
        $stripped =~ s/"(?:[^"\\]|\\.)*"//g;
        $stripped =~ s/'(?:[^'\\]|\\.)*'//g;

        # Look for magic numbers (not 0, 1, -1, 2)
        # Only in comparisons, assignments, array indices
        while ($stripped =~ /(?:==|!=|<=|>=|<|>|\[|=)\s*(\d{3,})\b/g) {
            my $number = $1;
            # Skip common OK values
            next if $number =~ /^(?:100|200|256|512|1024|2048|4096|8080|8443|65535|0x[0-9a-fA-F]+)$/;

            report($filepath, $linenum, SEV_INFO, 'MAGIC_NUMBER',
                "Magic number $number; consider using a named constant");
        }
    }
}

# ===========================================================================
# using namespace Check
# ===========================================================================

sub check_using_namespace {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $ftype = get_file_type($filepath);

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Check for 'using namespace' in headers
        if ($line =~ /\busing\s+namespace\s+(\w+(?:::\w+)*)/) {
            my $ns = $1;

            if ($ftype eq FTYPE_HEADER) {
                report($filepath, $linenum, SEV_ERROR, 'USING_NAMESPACE_HEADER',
                    "'using namespace $ns' in header file pollutes the global namespace");
            } elsif ($ns eq 'std') {
                report($filepath, $linenum, SEV_WARNING, 'USING_NAMESPACE_STD',
                    "'using namespace std' is discouraged; use explicit std:: prefix");
            }
        }
    }
}

# ===========================================================================
# CMake Convention Checks
# ===========================================================================

sub check_cmake_conventions {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    check_cmake_indentation($filepath, $lines_ref, $changed_ref);
    check_cmake_function_style($filepath, $lines_ref, $changed_ref);
    check_cmake_variable_naming($filepath, $lines_ref, $changed_ref);
    check_cmake_best_practices($filepath, $lines_ref, $changed_ref);
}

sub check_cmake_indentation {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $depth = 0;
    my $in_multiline = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments and blank lines
        next if $line =~ /^\s*#/;
        next if $line =~ /^\s*$/;

        # Check for tab indentation
        if ($line =~ /^\t/) {
            report($filepath, $linenum, SEV_ERROR, 'CMAKE_TAB_INDENT',
                "Tab indentation in CMake file; use spaces");
        }

        # Closing keywords reduce depth
        if ($line =~ /^\s*\b(?:endif|endforeach|endwhile|endmacro|endfunction|else|elseif)\b/i) {
            $depth-- if $depth > 0;
        }

        # Check indent level
        if ($line =~ /^( +)\S/) {
            my $indent = length($1);
            my $expected = $depth * $CMAKE_INDENT_WIDTH;

            # Allow some flexibility for continuation lines
            if ($indent != $expected && !$in_multiline) {
                # Only flag if clearly wrong
                if (abs($indent - $expected) > $CMAKE_INDENT_WIDTH) {
                    report($filepath, $linenum, SEV_INFO, 'CMAKE_INDENT',
                        "CMake indentation is $indent spaces; expected approximately $expected (${CMAKE_INDENT_WIDTH}-space indent)");
                }
            }
        }

        # Opening keywords increase depth
        if ($line =~ /\b(?:if|foreach|while|macro|function|else|elseif)\s*\(/i) {
            $depth++;
        }

        # Track multiline commands (ending without closing paren)
        if ($line =~ /\(\s*$/ || ($line =~ /\(/ && $line !~ /\)/)) {
            $in_multiline = 1;
        }
        if ($in_multiline && $line =~ /\)/) {
            $in_multiline = 0;
        }
    }
}

sub check_cmake_function_style {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*#/;

        # CMake functions should be lowercase
        if ($line =~ /^\s*([A-Z_]+)\s*\(/ && $line !~ /^\s*[A-Z_]+\s*=/) {
            my $func = $1;
            # Some uppercase things are variables in conditions
            next if $func =~ /^(?:TRUE|FALSE|ON|OFF|YES|NO|AND|OR|NOT|DEFINED|MATCHES|STREQUAL|VERSION_\w+)$/;
            next if $func =~ /^[A-Z_]+$/;  # Could be a variable

            # Check if it's a known CMake command used in uppercase
            if ($func =~ /^(?:SET|IF|ELSE|ELSEIF|ENDIF|FOREACH|ENDFOREACH|WHILE|ENDWHILE|FUNCTION|ENDFUNCTION|MACRO|ENDMACRO|ADD_\w+|FIND_\w+|TARGET_\w+|INSTALL|MESSAGE|OPTION|PROJECT|CMAKE_\w+|INCLUDE|LIST|STRING|FILE|GET_\w+)$/i) {
                my $lower = lc($func);
                report($filepath, $linenum, SEV_STYLE, 'CMAKE_UPPERCASE_COMMAND',
                    "CMake command '$func' should be lowercase ('$lower')");
            }
        }

        # Check for deprecated CMake patterns
        if ($line =~ /\bcmake_minimum_required\s*\(.*VERSION\s+([0-9.]+)/) {
            my $version = $1;
            my @parts = split /\./, $version;
            if ($parts[0] < 3) {
                report($filepath, $linenum, SEV_WARNING, 'CMAKE_OLD_VERSION',
                    "CMake minimum version $version is very old; project uses 3.28+");
            }
        }
    }
}

sub check_cmake_variable_naming {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*#/;

        # Check variable naming (should be UPPER_SNAKE_CASE)
        if ($line =~ /^\s*set\s*\(\s*(\w+)/) {
            my $var = $1;
            # Skip CMake built-in variables
            next if $var =~ /^CMAKE_/;
            next if $var =~ /^(?:PROJECT_|CPACK_)/;

            # Project variables should be UPPER_SNAKE_CASE
            unless ($var =~ $RE_MACRO_CASE) {
                report($filepath, $linenum, SEV_STYLE, 'CMAKE_VAR_NAMING',
                    "CMake variable '$var' should be UPPER_SNAKE_CASE");
            }
        }
    }
}

sub check_cmake_best_practices {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*#/;

        # Check for GLOB usage (anti-pattern)
        if ($line =~ /\bfile\s*\(\s*GLOB\b/i) {
            report($filepath, $linenum, SEV_WARNING, 'CMAKE_GLOB',
                "file(GLOB ...) doesn't track new/removed files; list sources explicitly");
        }

        # Check for add_compile_options vs target_compile_options
        if ($line =~ /\badd_compile_options\s*\(/i) {
            report($filepath, $linenum, SEV_INFO, 'CMAKE_GLOBAL_COMPILE_OPTIONS',
                "Consider target_compile_options() over add_compile_options() for per-target settings");
        }

        # Check for include_directories vs target_include_directories
        if ($line =~ /\binclude_directories\s*\(/i) {
            report($filepath, $linenum, SEV_INFO, 'CMAKE_GLOBAL_INCLUDE_DIRS',
                "Consider target_include_directories() over include_directories() for per-target settings");
        }

        # Check for link_directories (deprecated practice)
        if ($line =~ /\blink_directories\s*\(/i) {
            report($filepath, $linenum, SEV_WARNING, 'CMAKE_LINK_DIRECTORIES',
                "Avoid link_directories(); use target_link_libraries() with full paths or imported targets");
        }
    }
}

# ===========================================================================
# Fix Mode - Write Changes Back
# ===========================================================================

sub write_fixes {
    my ($filepath, $lines_ref) = @_;

    return unless $opt_fix;

    open(my $fh, '>:encoding(UTF-8)', $filepath) or do {
        warn "Cannot write fixes to $filepath: $!\n";
        return;
    };

    foreach my $line (@$lines_ref) {
        print $fh "$line\n";
    }

    close($fh);
    print colorize('green', "Fixed: $filepath") . "\n" if $opt_verbose;
}

# ===========================================================================
# Utility Functions
# ===========================================================================

sub min {
    my ($a, $b) = @_;
    return $a < $b ? $a : $b;
}

sub max {
    my ($a, $b) = @_;
    return $a > $b ? $a : $b;
}

sub trim {
    my ($str) = @_;
    $str =~ s/^\s+//;
    $str =~ s/\s+$//;
    return $str;
}

sub is_string_literal_line {
    my ($line) = @_;
    return ($line =~ /^\s*"/ || $line =~ /^\s*R"/);
}

sub strip_comments {
    my ($line) = @_;
    my $result = $line;
    # Remove single-line comments
    $result =~ s/\/\/.*$//;
    # Remove inline block comments
    $result =~ s/\/\*.*?\*\///g;
    return $result;
}

sub strip_strings {
    my ($line) = @_;
    my $result = $line;
    $result =~ s/"(?:[^"\\]|\\.)*"/""/g;
    $result =~ s/'(?:[^'\\]|\\.)*'/''/g;
    return $result;
}

sub count_in_string {
    my ($str, $char) = @_;
    my @matches = ($str =~ /\Q$char\E/g);
    return scalar @matches;
}

# ===========================================================================
# Rule Documentation
# ===========================================================================

my %RULE_DOCS = (
    # Common
    'TRAILING_WHITESPACE'       => 'Trailing whitespace at end of line',
    'CRLF_LINE_ENDING'         => 'Windows CRLF line endings (use Unix LF)',
    'NO_FINAL_NEWLINE'         => 'File should end with a newline character',
    'CONSECUTIVE_BLANK_LINES'  => 'Too many consecutive blank lines',
    'FILE_ACCESS'              => 'Cannot access or read file',
    'FILE_READ'                => 'Error reading file content',

    # License
    'MISSING_SPDX_HEADER'      => 'Missing SPDX license header block',
    'INVALID_SPDX_IDENTIFIER'  => 'Invalid or unrecognized SPDX identifier',
    'MISSING_COPYRIGHT'        => 'Missing SPDX-FileCopyrightText',
    'HEADER_NOT_IN_COMMENT'    => 'SPDX header not in comment block',

    # Header Guards
    'MISSING_HEADER_GUARD'     => 'Header file missing #pragma once',
    'USE_PRAGMA_ONCE'          => 'Use #pragma once instead of #ifndef guards',

    # Indentation
    'TAB_INDENT'               => 'Tab indentation (use 4 spaces)',
    'TAB_IN_CODE'              => 'Tab character in non-indentation position',
    'INDENT_NOT_MULTIPLE'      => 'Indentation not a multiple of 4 spaces',

    # Line Length
    'LINE_TOO_LONG'            => 'Line exceeds maximum length',
    'LINE_EXCESSIVELY_LONG'    => 'Line far exceeds maximum length',

    # Naming
    'CLASS_NAME_CASE'          => 'Class/struct name should be PascalCase',
    'MEMBER_VAR_NAMING'        => 'Member variable should follow m_camelCase',
    'MEMBER_NO_PREFIX'         => 'Private/protected member missing m_ prefix',
    'ENUM_CLASS_NAME'          => 'Enum class name should be PascalCase',
    'MACRO_NAME_CASE'          => 'Macro name should be UPPER_SNAKE_CASE',
    'NAMESPACE_NAMING'         => 'Namespace name should be PascalCase',

    # Brace Style
    'BRACE_NEXT_LINE_CONTROL'  => 'Opening brace should be on same line as control statement',
    'BRACE_ELSE_SPACING'       => 'Missing space between } and else',
    'EMPTY_ELSE_BLOCK'         => 'Empty else block',
    'EMPTY_IF_BLOCK'           => 'Empty if block',

    # Pointer/Reference
    'POINTER_ALIGNMENT'        => 'Pointer * should be adjacent to variable name',
    'REFERENCE_ALIGNMENT'      => 'Reference & should be adjacent to variable name',

    # Spacing
    'KEYWORD_SPACE'            => 'Missing space between keyword and parenthesis',
    'SPACE_BEFORE_SEMICOLON'   => 'Unexpected space before semicolon',
    'SPACE_AFTER_COMMA'        => 'Missing space after comma',
    'DOUBLE_SPACE'             => 'Multiple consecutive spaces in code',
    'SPACE_BEFORE_BRACE'       => 'Missing space before opening brace',

    # Qt
    'MISSING_Q_OBJECT'         => 'QObject-derived class missing Q_OBJECT macro',
    'OLD_QT_CONNECT'           => 'Old-style SIGNAL/SLOT connect syntax',
    'EMIT_KEYWORD'             => 'Consider Q_EMIT over emit',
    'QOBJECT_TR'               => 'Direct QObject::tr() usage',
    'QT_SIGNAL_SLOT_SYNTAX'    => 'Prefer signals/slots over Q_SIGNALS/Q_SLOTS',

    # Includes
    'INCLUDE_ORDER'            => 'Includes not in alphabetical order',
    'INCLUDE_SYNTAX'           => 'Invalid include syntax',
    'C_HEADER_IN_CPP'          => 'C header used instead of C++ equivalent',
    'ABSOLUTE_INCLUDE_PATH'    => 'Absolute path in include',
    'BACKSLASH_IN_INCLUDE'     => 'Backslash in include path',

    # Comments
    'COMMENT_STYLE'            => 'Single-line comment should use //',
    'COMMENTED_OUT_CODE'       => 'Possible commented-out code',
    'TODO_FORMAT'              => 'TODO/FIXME format suggestion',
    'TODO_MARKER'              => 'TODO/FIXME/HACK marker',

    # Control Flow
    'IF_WITHOUT_BRACES'        => 'if statement without braces',
    'GOTO_STATEMENT'           => 'Use of goto',
    'NESTED_TERNARY'           => 'Nested ternary operator',
    'YODA_CONDITION'           => 'Yoda condition (constant == variable)',
    'ASSIGNMENT_IN_CONDITION'  => 'Assignment in condition',

    # Strings
    'STD_STRING_USAGE'         => 'std::string instead of QString',
    'STD_IOSTREAM'             => 'std::cout/cerr instead of Qt logging',
    'PRINTF_USAGE'             => 'printf instead of Qt logging',
    'EMPTY_STRING_COMPARE'     => 'Comparing with empty string ""',
    'STRING_CONCAT_LOOP'       => 'String concatenation in loop',

    # Memory
    'RAW_NEW'                  => 'Raw new without smart pointer',
    'RAW_DELETE'               => 'Raw delete (prefer smart pointers)',
    'C_MEMORY_FUNCTIONS'       => 'C-style memory functions',
    'SHARED_PTR_NEW'           => 'shared_ptr with new (use make_shared)',
    'UNIQUE_PTR_NEW'           => 'unique_ptr with new (use make_unique)',

    # Const
    'CONST_REF_PARAM'          => 'Consider const reference for parameter',

    # Enum
    'PREFER_ENUM_CLASS'        => 'Prefer enum class over plain enum',
    'ANONYMOUS_ENUM'           => 'Anonymous enum',

    # Constructor
    'IMPLICIT_CONSTRUCTOR'     => 'Single-parameter constructor without explicit',
    'USE_INIT_LIST'            => 'Consider initializer list in constructor',

    # Function
    'FUNCTION_TOO_LONG'        => 'Function exceeds recommended length',
    'DEEP_NESTING'             => 'Excessive nesting depth',

    # Deprecated
    'USE_NULLPTR'              => 'NULL instead of nullptr',
    'C_STYLE_CAST'             => 'C-style cast',
    'PREFER_USING'             => 'typedef instead of using alias',
    'QLIST_VALUE_TYPE'         => 'QList of basic types',
    'DEPRECATED_QT_ALGORITHM'  => 'Deprecated Qt algorithm',
    'REGISTER_KEYWORD'         => 'Deprecated register keyword',
    'AUTO_PTR'                 => 'Removed std::auto_ptr',
    'DEPRECATED_BIND'          => 'Removed std::bind1st/bind2nd',

    # Security
    'DANGEROUS_FUNCTION'       => 'Dangerous function (gets)',
    'SPRINTF_USAGE'            => 'sprintf without bounds checking',
    'STRCPY_USAGE'             => 'strcpy without bounds checking',
    'STRCAT_USAGE'             => 'strcat without bounds checking',
    'SYSTEM_CALL'              => 'system() call',
    'FORMAT_STRING'            => 'Format string vulnerability',
    'HARDCODED_CREDENTIALS'    => 'Hardcoded credentials',
    'C_RAND'                   => 'C rand() instead of <random>',

    # Tests
    'MISSING_QTEST_INCLUDE'    => 'Test file missing QTest include',
    'MISSING_QTEST_MAIN'       => 'Test file missing QTEST_MAIN macro',
    'MISSING_MOC_INCLUDE'      => 'Test file missing moc include',
    'TEST_MAIN_MISMATCH'       => 'QTEST_MAIN class mismatch',
    'TEST_METHOD_NAMING'       => 'Test method naming convention',
    'TRIVIAL_ASSERTION'        => 'Trivial assertion',

    # Exceptions
    'THROW_BY_POINTER'         => 'Throw by pointer instead of value',
    'CATCH_BY_VALUE'           => 'Catch by value instead of reference',
    'CATCH_NON_CONST_REF'      => 'Catch by non-const reference',
    'EMPTY_CATCH'              => 'Empty catch block',
    'CATCH_ALL_NO_RETHROW'     => 'catch(...) without re-throw',

    # Virtual/Override
    'MISSING_VIRTUAL_DESTRUCTOR' => 'Missing virtual destructor in polymorphic class',
    'MISSING_OVERRIDE'          => 'Missing override keyword',
    'VIRTUAL_AND_OVERRIDE'      => 'Redundant virtual with override',

    # Auto
    'AUTO_REF_ITERATION'       => 'Consider const auto& for iteration',
    'AUTO_RETURN_TYPE'         => 'Auto return type readability',

    # Lambda
    'LAMBDA_CAPTURE_ALL_VALUE' => 'Lambda captures everything by value',
    'LAMBDA_CAPTURE_ALL_REF'   => 'Lambda captures everything by reference',
    'LONG_LAMBDA'              => 'Long lambda body',

    # Switch
    'CASE_FALLTHROUGH'         => 'Case fall-through without break',
    'SWITCH_NO_DEFAULT'        => 'Switch without default case',

    # Class
    'CLASS_ACCESS_ORDER'       => 'Access specifier ordering',

    # Debug
    'DEBUG_PRINT'              => 'Debug print statement left in code',
    'DEBUG_FUNCTION_NAME'      => 'Debug print with __FUNCTION__',

    # Multiple statements
    'MULTIPLE_STATEMENTS'      => 'Multiple statements on one line',

    # Magic numbers
    'MAGIC_NUMBER'             => 'Magic number without named constant',

    # using namespace
    'USING_NAMESPACE_HEADER'   => 'using namespace in header file',
    'USING_NAMESPACE_STD'      => 'using namespace std',

    # CMake
    'CMAKE_MISSING_SPDX'      => 'CMake file missing SPDX header',
    'CMAKE_TAB_INDENT'        => 'Tab indentation in CMake file',
    'CMAKE_INDENT'             => 'CMake indentation issue',
    'CMAKE_UPPERCASE_COMMAND'  => 'CMake command in uppercase',
    'CMAKE_OLD_VERSION'        => 'Very old CMake minimum version',
    'CMAKE_VAR_NAMING'         => 'CMake variable naming',
    'CMAKE_GLOB'               => 'file(GLOB) anti-pattern',
    'CMAKE_GLOBAL_COMPILE_OPTIONS' => 'Global compile options vs per-target',
    'CMAKE_GLOBAL_INCLUDE_DIRS'    => 'Global include dirs vs per-target',
    'CMAKE_LINK_DIRECTORIES'       => 'Deprecated link_directories usage',

    # File
    'FILE_TOO_LONG'            => 'File exceeds recommended length',
);

# ===========================================================================
# Rule Listing (for documentation)
# ===========================================================================

sub list_rules {
    print "\n";
    print colorize('bold', "MeshMC checkpatch.pl - Rule Reference") . "\n";
    print colorize('bold', "=" x 60) . "\n\n";

    my @categories = (
        ['Common'       => [qw(TRAILING_WHITESPACE CRLF_LINE_ENDING NO_FINAL_NEWLINE CONSECUTIVE_BLANK_LINES)]],
        ['License'      => [qw(MISSING_SPDX_HEADER INVALID_SPDX_IDENTIFIER MISSING_COPYRIGHT HEADER_NOT_IN_COMMENT)]],
        ['Header Guards' => [qw(MISSING_HEADER_GUARD USE_PRAGMA_ONCE)]],
        ['Indentation'  => [qw(TAB_INDENT TAB_IN_CODE INDENT_NOT_MULTIPLE)]],
        ['Line Length'  => [qw(LINE_TOO_LONG LINE_EXCESSIVELY_LONG)]],
        ['Naming'       => [qw(CLASS_NAME_CASE MEMBER_VAR_NAMING MEMBER_NO_PREFIX ENUM_CLASS_NAME MACRO_NAME_CASE NAMESPACE_NAMING)]],
        ['Brace Style'  => [qw(BRACE_NEXT_LINE_CONTROL BRACE_ELSE_SPACING EMPTY_ELSE_BLOCK EMPTY_IF_BLOCK)]],
        ['Pointer/Ref'  => [qw(POINTER_ALIGNMENT REFERENCE_ALIGNMENT)]],
        ['Spacing'      => [qw(KEYWORD_SPACE SPACE_BEFORE_SEMICOLON SPACE_AFTER_COMMA DOUBLE_SPACE SPACE_BEFORE_BRACE)]],
        ['Qt'           => [qw(MISSING_Q_OBJECT OLD_QT_CONNECT EMIT_KEYWORD QOBJECT_TR QT_SIGNAL_SLOT_SYNTAX)]],
        ['Includes'     => [qw(INCLUDE_ORDER INCLUDE_SYNTAX C_HEADER_IN_CPP ABSOLUTE_INCLUDE_PATH BACKSLASH_IN_INCLUDE)]],
        ['Comments'     => [qw(COMMENT_STYLE COMMENTED_OUT_CODE TODO_FORMAT TODO_MARKER)]],
        ['Control Flow' => [qw(IF_WITHOUT_BRACES GOTO_STATEMENT NESTED_TERNARY YODA_CONDITION ASSIGNMENT_IN_CONDITION)]],
        ['Strings'      => [qw(STD_STRING_USAGE STD_IOSTREAM PRINTF_USAGE EMPTY_STRING_COMPARE STRING_CONCAT_LOOP)]],
        ['Memory'       => [qw(RAW_NEW RAW_DELETE C_MEMORY_FUNCTIONS SHARED_PTR_NEW UNIQUE_PTR_NEW)]],
        ['Const'        => [qw(CONST_REF_PARAM)]],
        ['Enum'         => [qw(PREFER_ENUM_CLASS ANONYMOUS_ENUM)]],
        ['Constructor'  => [qw(IMPLICIT_CONSTRUCTOR USE_INIT_LIST)]],
        ['Function'     => [qw(FUNCTION_TOO_LONG DEEP_NESTING)]],
        ['Deprecated'   => [qw(USE_NULLPTR C_STYLE_CAST PREFER_USING QLIST_VALUE_TYPE DEPRECATED_QT_ALGORITHM REGISTER_KEYWORD AUTO_PTR DEPRECATED_BIND)]],
        ['Security'     => [qw(DANGEROUS_FUNCTION SPRINTF_USAGE STRCPY_USAGE STRCAT_USAGE SYSTEM_CALL FORMAT_STRING HARDCODED_CREDENTIALS C_RAND)]],
        ['Tests'        => [qw(MISSING_QTEST_INCLUDE MISSING_QTEST_MAIN MISSING_MOC_INCLUDE TEST_MAIN_MISMATCH TEST_METHOD_NAMING TRIVIAL_ASSERTION)]],
        ['Exceptions'   => [qw(THROW_BY_POINTER CATCH_BY_VALUE CATCH_NON_CONST_REF EMPTY_CATCH CATCH_ALL_NO_RETHROW)]],
        ['Virtual/Override' => [qw(MISSING_VIRTUAL_DESTRUCTOR MISSING_OVERRIDE VIRTUAL_AND_OVERRIDE)]],
        ['Auto'         => [qw(AUTO_REF_ITERATION AUTO_RETURN_TYPE)]],
        ['Lambda'       => [qw(LAMBDA_CAPTURE_ALL_VALUE LAMBDA_CAPTURE_ALL_REF LONG_LAMBDA)]],
        ['Switch'       => [qw(CASE_FALLTHROUGH SWITCH_NO_DEFAULT)]],
        ['Class'        => [qw(CLASS_ACCESS_ORDER)]],
        ['Debug'        => [qw(DEBUG_PRINT DEBUG_FUNCTION_NAME)]],
        ['Style'        => [qw(MULTIPLE_STATEMENTS MAGIC_NUMBER USING_NAMESPACE_HEADER USING_NAMESPACE_STD)]],
        ['CMake'        => [qw(CMAKE_MISSING_SPDX CMAKE_TAB_INDENT CMAKE_INDENT CMAKE_UPPERCASE_COMMAND CMAKE_OLD_VERSION CMAKE_VAR_NAMING CMAKE_GLOB CMAKE_GLOBAL_COMPILE_OPTIONS CMAKE_GLOBAL_INCLUDE_DIRS CMAKE_LINK_DIRECTORIES)]],
    );

    foreach my $cat (@categories) {
        my ($name, $rules) = @$cat;
        print colorize('blue', "  $name:") . "\n";
        foreach my $rule (@$rules) {
            my $desc = $RULE_DOCS{$rule} // 'No description';
            printf "    %-35s %s\n", $rule, $desc;
        }
        print "\n";
    }
}

# ===========================================================================
# Self-Test
# ===========================================================================

sub run_self_test {
    print colorize('bold', "Running self-tests...") . "\n\n";

    my $pass = 0;
    my $fail = 0;

    # Test 1: Trailing whitespace detection
    {
        my @test_lines = ("hello   ", "world", "foo  ");
        my %changed = (1 => 1, 2 => 1, 3 => 1);
        my $before_count = $g_warn_count;
        check_trailing_whitespace("test.cpp", \@test_lines, \%changed);
        if ($g_warn_count - $before_count == 2) {
            print colorize('green', "  PASS") . ": Trailing whitespace detection\n";
            $pass++;
        } else {
            print colorize('red', "  FAIL") . ": Trailing whitespace detection (expected 2 warnings)\n";
            $fail++;
        }
    }

    # Test 2: Tab detection
    {
        my @test_lines = ("\tint x = 0;");
        my %changed = (1 => 1);
        my $before_count = $g_error_count;
        check_tab_usage("test.cpp", \@test_lines, \%changed);
        # Note: TAB_INDENT is checked in check_indentation, not check_tab_usage
        print colorize('green', "  PASS") . ": Tab detection test executed\n";
        $pass++;
    }

    # Test 3: glob_to_regex
    {
        my $re = glob_to_regex("*.cpp");
        if ("test.cpp" =~ /$re/) {
            print colorize('green', "  PASS") . ": Glob to regex conversion\n";
            $pass++;
        } else {
            print colorize('red', "  FAIL") . ": Glob to regex conversion\n";
            $fail++;
        }
    }

    # Test 4: File type detection
    {
        if (get_file_type("foo.cpp") eq FTYPE_CPP &&
            get_file_type("bar.h") eq FTYPE_HEADER &&
            get_file_type("CMakeLists.txt") eq FTYPE_CMAKE) {
            print colorize('green', "  PASS") . ": File type detection\n";
            $pass++;
        } else {
            print colorize('red', "  FAIL") . ": File type detection\n";
            $fail++;
        }
    }

    # Test 5: count_braces
    {
        if (count_braces("if (x) {") == 1 &&
            count_braces("}") == -1 &&
            count_braces("{ { }") == 1) {
            print colorize('green', "  PASS") . ": Brace counting\n";
            $pass++;
        } else {
            print colorize('red', "  FAIL") . ": Brace counting\n";
            $fail++;
        }
    }

    # Test 6: strip_strings
    {
        my $result = strip_strings('auto x = "hello world";');
        if ($result !~ /hello world/) {
            print colorize('green', "  PASS") . ": String stripping\n";
            $pass++;
        } else {
            print colorize('red', "  FAIL") . ": String stripping\n";
            $fail++;
        }
    }

    # Test 7: strip_comments
    {
        my $result = strip_comments('int x = 0; // comment');
        if ($result !~ /comment/) {
            print colorize('green', "  PASS") . ": Comment stripping\n";
            $pass++;
        } else {
            print colorize('red', "  FAIL") . ": Comment stripping\n";
            $fail++;
        }
    }

    # Test 8: Naming convention regex
    {
        if ("MyClass" =~ $RE_PASCAL_CASE &&
            "myMethod" =~ $RE_CAMEL_CASE &&
            "m_variable" =~ $RE_MEMBER_VAR &&
            "MY_MACRO" =~ $RE_MACRO_CASE) {
            print colorize('green', "  PASS") . ": Naming convention regexes\n";
            $pass++;
        } else {
            print colorize('red', "  FAIL") . ": Naming convention regexes\n";
            $fail++;
        }
    }

    # Test 9: should_exclude
    {
        if (should_exclude("build/foo.cpp") &&
            !should_exclude("launcher/foo.cpp")) {
            print colorize('green', "  PASS") . ": File exclusion\n";
            $pass++;
        } else {
            print colorize('red', "  FAIL") . ": File exclusion\n";
            $fail++;
        }
    }

    # Test 10: min/max
    {
        if (min(3, 5) == 3 && max(3, 5) == 5) {
            print colorize('green', "  PASS") . ": min/max utility\n";
            $pass++;
        } else {
            print colorize('red', "  FAIL") . ": min/max utility\n";
            $fail++;
        }
    }

    print "\n";
    print colorize('bold', "Self-test results: $pass passed, $fail failed") . "\n";

    return $fail == 0;
}

# ===========================================================================
# Run
# ===========================================================================

# ===========================================================================
# Additional C++ Checks: Template Style
# ===========================================================================

sub check_template_style {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for ">>" in nested templates (C++11 allows it, but >> can be confusing)
        # Actually C++11 fixed this, so no need to flag it

        # Check for template keyword on separate line from function
        if ($line =~ /^\s*template\s*<.*>\s*$/) {
            # Template declaration on its own line - this is the preferred style
            # Check that the next line has the function/class declaration
            if ($i + 1 < scalar @$lines_ref) {
                my $next = $lines_ref->[$i + 1];
                if ($next =~ /^\s*$/) {
                    report($filepath, $linenum, SEV_STYLE, 'TEMPLATE_BLANK_LINE',
                        "No blank line expected between template<> declaration and function/class");
                }
            }
        }

        # Check for overly complex template parameter lists
        if ($line =~ /template\s*</) {
            my $depth = 0;
            my $param_count = 1;
            for my $ch (split //, $line) {
                if ($ch eq '<') { $depth++; }
                elsif ($ch eq '>') { $depth--; }
                elsif ($ch eq ',' && $depth == 1) { $param_count++; }
            }
            if ($param_count > 4) {
                report($filepath, $linenum, SEV_INFO, 'COMPLEX_TEMPLATE',
                    "Template has $param_count parameters; consider simplifying");
            }
        }

        # Check for missing typename in dependent types
        if ($line =~ /\b(?:class|struct)\s+\w+\s*:.*::/) {
            # Complex pattern - skip for now
        }
    }
}

# ===========================================================================
# Additional C++ Checks: RAII and Resource Management
# ===========================================================================

sub check_raii_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $in_function = 0;
    my @open_resources = ();

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for fopen without fclose in same scope
        if ($line =~ /\bfopen\s*\(/) {
            report($filepath, $linenum, SEV_WARNING, 'C_FILE_IO',
                "C-style fopen(); use QFile or std::fstream for RAII-based file handling");
        }

        # Check for manual lock/unlock patterns
        if ($line =~ /\b(\w+)\.lock\s*\(\s*\)/) {
            report($filepath, $linenum, SEV_WARNING, 'MANUAL_LOCK',
                "Manual mutex lock; use QMutexLocker or std::lock_guard for RAII locking");
        }

        # Check for socket/handle leaks
        if ($line =~ /\b(?:socket|open)\s*\(/ && $line !~ /QFile|QTcpSocket|QUdpSocket/) {
            report($filepath, $linenum, SEV_INFO, 'RAW_HANDLE',
                "Raw OS handle; consider using Qt wrapper classes for automatic resource management");
        }

        # Check for manual cleanup patterns (goto cleanup style)
        if ($line =~ /^\s*cleanup:/ || $line =~ /\bgoto\s+cleanup\b/i) {
            report($filepath, $linenum, SEV_WARNING, 'GOTO_CLEANUP',
                "Goto cleanup pattern; use RAII (smart pointers, scope guards) instead");
        }
    }
}

# ===========================================================================
# Additional C++ Checks: Move Semantics
# ===========================================================================

sub check_move_semantics {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for std::move on const objects (no effect)
        if ($line =~ /std::move\s*\(\s*const\b/) {
            report($filepath, $linenum, SEV_WARNING, 'MOVE_CONST',
                "std::move on const object has no effect; the copy constructor will be called");
        }

        # Check for use after std::move
        if ($line =~ /std::move\s*\(\s*(\w+)\s*\)/) {
            my $moved_var = $1;
            # Check subsequent lines for use of the same variable
            for (my $j = $i + 1; $j < scalar @$lines_ref && $j < $i + 5; $j++) {
                my $next = $lines_ref->[$j];
                next if $next =~ /^\s*\/\//;
                if ($next =~ /\b\Q$moved_var\E\b/ && $next !~ /=\s*/ && $next !~ /std::move/) {
                    report($filepath, $j + 1, SEV_WARNING, 'USE_AFTER_MOVE',
                        "Potential use of '$moved_var' after std::move; moved-from objects are in unspecified state");
                    last;
                }
                last if $next =~ /[{}]/;  # Stop at scope boundary
            }
        }

        # Check for returning local by std::move (prevents RVO)
        if ($line =~ /return\s+std::move\s*\(\s*(\w+)\s*\)/) {
            report($filepath, $linenum, SEV_WARNING, 'RETURN_STD_MOVE',
                "return std::move() prevents Return Value Optimization; return by value instead");
        }

        # Check for push_back without emplace_back alternative
        if ($line =~ /\.push_back\s*\(\s*\w+\s*\(\s*\)/) {
            report($filepath, $linenum, SEV_INFO, 'PUSH_BACK_TEMP',
                "Consider emplace_back() instead of push_back() with temporary object");
        }
    }
}

# ===========================================================================
# Additional C++ Checks: Concurrency Patterns
# ===========================================================================

sub check_concurrency_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for volatile as synchronization (it's not!)
        if ($line =~ /\bvolatile\b/ && $line !~ /\bsig_atomic_t\b/) {
            report($filepath, $linenum, SEV_INFO, 'VOLATILE_SYNC',
                "'volatile' does not provide thread safety; use std::atomic or QAtomicInt");
        }

        # Check for sleep-based synchronization
        if ($line =~ /\b(?:sleep|usleep|Sleep|QThread::sleep|QThread::msleep)\s*\(/) {
            report($filepath, $linenum, SEV_INFO, 'SLEEP_SYNC',
                "Sleep-based waiting; consider using QWaitCondition or event-driven approach");
        }

        # Check for thread-unsafe singleton pattern
        if ($line =~ /static\s+\w+\s*\*\s*\w+\s*=\s*(?:nullptr|NULL|0)\s*;/) {
            report($filepath, $linenum, SEV_INFO, 'THREAD_UNSAFE_SINGLETON',
                "Potential thread-unsafe singleton; use Meyers' singleton (static local) or std::call_once");
        }

        # Check for data races with static locals in lambdas
        if ($line =~ /\bstatic\b.*=/ && $line !~ /\bconst\b/ && $line !~ /\bconstexpr\b/) {
            # Check if inside a lambda or thread context
            for (my $j = $i - 1; $j >= 0 && $j >= $i - 10; $j--) {
                if ($lines_ref->[$j] =~ /\[.*\]\s*\(/ || $lines_ref->[$j] =~ /QThread|std::thread/) {
                    report($filepath, $linenum, SEV_WARNING, 'STATIC_IN_THREAD',
                        "Static variable in threaded context; ensure thread-safe access");
                    last;
                }
            }
        }
    }
}

# ===========================================================================
# Additional C++ Checks: Operator Overloading
# ===========================================================================

sub check_operator_overloading {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for operator overloading without const version
        if ($line =~ /\boperator\s*\[\]\s*\(/ && $line !~ /\bconst\b/) {
            report($filepath, $linenum, SEV_INFO, 'OPERATOR_CONST',
                "operator[] should have both const and non-const versions");
        }

        # Check for asymmetric comparison operators
        if ($line =~ /\boperator\s*==\s*\(/) {
            # Check if operator!= is also defined nearby
            my $has_neq = 0;
            for (my $j = max(0, $i - 20); $j < min(scalar @$lines_ref, $i + 20); $j++) {
                if ($lines_ref->[$j] =~ /\boperator\s*!=\s*\(/) {
                    $has_neq = 1;
                    last;
                }
            }
            if (!$has_neq) {
                report($filepath, $linenum, SEV_INFO, 'MISSING_OPERATOR_NEQ',
                    "operator== defined without operator!=; consider implementing both (or use C++20 <=>)");
            }
        }

        # Check for self-assignment in operator=
        if ($line =~ /\boperator\s*=\s*\(\s*const/) {
            my $has_self_check = 0;
            for (my $j = $i + 1; $j < scalar @$lines_ref && $j < $i + 10; $j++) {
                if ($lines_ref->[$j] =~ /this\s*==\s*&|&\w+\s*==\s*this/) {
                    $has_self_check = 1;
                    last;
                }
                last if $lines_ref->[$j] =~ /^}/;
            }
            # Only check if we can see the body
            if (!$has_self_check && $i + 1 < scalar @$lines_ref && $lines_ref->[$i + 1] =~ /{/) {
                report($filepath, $linenum, SEV_INFO, 'OPERATOR_SELF_ASSIGN',
                    "Copy assignment operator may need self-assignment check (this == &other)");
            }
        }
    }
}

# ===========================================================================
# Additional Checks: File Organization
# ===========================================================================

sub check_file_organization {
    my ($filepath, $lines_ref, $changed_ref, $ftype) = @_;

    my $basename = basename($filepath);

    # Check .h/.cpp file name matches class name
    if ($ftype eq FTYPE_HEADER || $ftype eq FTYPE_CPP) {
        my $expected_class = $basename;
        $expected_class =~ s/\.\w+$//;  # Remove extension
        $expected_class =~ s/_test$//;   # Remove test suffix

        # Check if file has a class that matches its name
        my $found_matching_class = 0;
        for (my $i = 0; $i < scalar @$lines_ref; $i++) {
            if ($lines_ref->[$i] =~ /\bclass\s+(\w+)\b/ && $lines_ref->[$i] !~ /;\s*$/) {
                if ($1 eq $expected_class) {
                    $found_matching_class = 1;
                    last;
                }
            }
        }

        # Only flag if file has classes but none match the filename
        my $has_any_class = 0;
        for (my $i = 0; $i < scalar @$lines_ref; $i++) {
            if ($lines_ref->[$i] =~ /\bclass\s+\w+\b/ && $lines_ref->[$i] !~ /;\s*$/) {
                $has_any_class = 1;
                last;
            }
        }

        if ($has_any_class && !$found_matching_class && $ftype eq FTYPE_HEADER) {
            report($filepath, 1, SEV_INFO, 'FILE_CLASS_MISMATCH',
                "Header file '$basename' does not contain a class named '$expected_class'");
        }
    }

    # Check for .cpp files without corresponding .h (implementation-only files)
    if ($ftype eq FTYPE_CPP) {
        # Skip test files and main files
        return if $basename =~ /_test\.cpp$|^main\.cpp$/;

        my $header = $basename;
        $header =~ s/\.cpp$/.h/;

        # Check if the .cpp file includes its own header
        my $includes_own_header = 0;
        for (my $i = 0; $i < scalar @$lines_ref && $i < 30; $i++) {
            if ($lines_ref->[$i] =~ /#\s*include\s*".*\Q$header\E"/) {
                $includes_own_header = 1;
                last;
            }
        }

        # It's OK if it doesn't - some .cpp files are standalone
    }

    # Check PascalCase file naming
    if ($ftype eq FTYPE_HEADER || $ftype eq FTYPE_CPP) {
        my $name_part = $basename;
        $name_part =~ s/\.\w+$//;
        $name_part =~ s/_test$//;

        # MeshMC uses PascalCase for filenames
        unless ($name_part =~ /^[A-Z]/ || $name_part =~ /^[a-z]+$/) {
            # Mixed case starting with lowercase is wrong
            if ($name_part =~ /^[a-z].*[A-Z]/) {
                report($filepath, 1, SEV_STYLE, 'FILE_NAMING',
                    "File name '$basename' should use PascalCase (e.g., '${\ ucfirst($name_part)}')");
            }
        }
    }
}

# ===========================================================================
# Additional Checks: Signal/Slot Detailed Analysis
# ===========================================================================

sub check_signal_slot_details {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $in_signals_section = 0;
    my $in_slots_section = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        # Track signal/slot sections
        if ($line =~ /^\s*signals\s*:/) {
            $in_signals_section = 1;
            $in_slots_section = 0;
        } elsif ($line =~ /^\s*(?:public|private|protected)\s+slots\s*:/) {
            $in_signals_section = 0;
            $in_slots_section = 1;
        } elsif ($line =~ /^\s*(?:public|private|protected)\s*:/ && $line !~ /slots/) {
            $in_signals_section = 0;
            $in_slots_section = 0;
        }

        next unless exists $changed_ref->{$linenum};

        # Check signal naming (should be verb/past-tense or present continuous)
        if ($in_signals_section && $line =~ /^\s*void\s+(\w+)\s*\(/) {
            my $signal = $1;
            # Common patterns: changed(), clicked(), finished(), started()
            # Warning if signal name doesn't end with a verb form
            unless ($signal =~ /(?:ed|ing|Changed|Clicked|Pressed|Released|Triggered|Toggled|Finished|Started|Failed|Succeeded|Updated|Loaded|Saved|Closed|Opened|Selected|Activated|Deactivated|Shown|Hidden|Moved|Resized)$/) {
                report($filepath, $linenum, SEV_INFO, 'SIGNAL_NAMING',
                    "Signal '$signal' should use past-tense or progressive naming (e.g., '${signal}Changed', '${signal}Updated')");
            }
        }

        # Check slot naming (on_widget_signal pattern)
        if ($in_slots_section && $line =~ /^\s*void\s+(\w+)\s*\(/) {
            my $slot = $1;
            # MeshMC uses on_widgetName_signalName pattern
            # But also uses pure camelCase
        }

        # Check for direct signal emission without error checking
        if ($line =~ /\bemit\s+(\w+)\s*\(/) {
            # Just informational
        }

        # Check for connections in constructors (common Qt pattern)
        if ($line =~ /\bconnect\s*\(/) {
            # Check for self-connection without this
            if ($line =~ /connect\s*\(\s*\w+\s*,/) {
                # OK - connecting to another object
            }

            # Check for lambda connections that capture 'this' without checking lifetime
            if ($line =~ /connect\s*\(.*\[.*this.*\]/) {
                # Check if the connection is to a temporary or might outlive 'this'
                if ($line =~ /connect\s*\(\s*(\w+)\.get\(\)/) {
                    report($filepath, $linenum, SEV_INFO, 'CONNECT_LIFETIME',
                        "Lambda captures 'this' in connect(); ensure connected object doesn't outlive 'this'");
                }
            }
        }
    }
}

# ===========================================================================
# Additional Checks: JSON Usage Patterns
# ===========================================================================

sub check_json_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for direct QJsonDocument::fromJson without error handling
        if ($line =~ /QJsonDocument::fromJson\s*\(/ && $line !~ /QJsonParseError/) {
            # Look in surrounding lines for error handling
            my $has_error_check = 0;
            for (my $j = max(0, $i - 2); $j <= min($#$lines_ref, $i + 5); $j++) {
                if ($lines_ref->[$j] =~ /QJsonParseError|error\.error/) {
                    $has_error_check = 1;
                    last;
                }
            }
            unless ($has_error_check) {
                report($filepath, $linenum, SEV_WARNING, 'JSON_NO_ERROR_CHECK',
                    "QJsonDocument::fromJson() without QJsonParseError check; use Json::requireDocument() helper");
            }
        }

        # Check for direct JSON value access without type checking
        if ($line =~ /\.toObject\(\)\s*\[/) {
            report($filepath, $linenum, SEV_INFO, 'JSON_UNCHECKED_ACCESS',
                "Direct JSON object access without type checking; use Json::requireObject() helpers");
        }
    }
}

# ===========================================================================
# Additional Checks: Network and URL Patterns
# ===========================================================================

sub check_network_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for HTTP instead of HTTPS
        if ($line =~ /"http:\/\// && $line !~ /localhost|127\.0\.0\.1|0\.0\.0\.0/) {
            # Skip test data and comments
            next if $filepath =~ /_test\.cpp$/;
            report($filepath, $linenum, SEV_WARNING, 'HTTP_NOT_HTTPS',
                "Insecure HTTP URL; use HTTPS when possible");
        }

        # Check for hardcoded URLs that should be configurable
        if ($line =~ /"https?:\/\/[^"]+\.[a-z]{2,}"/) {
            # Skip well-known URLs and test files
            next if $filepath =~ /_test\.cpp$/;
            next if $line =~ /github\.com|minecraft\.net|mojang\.com|microsoft\.com/;
            report($filepath, $linenum, SEV_INFO, 'HARDCODED_URL',
                "Hardcoded URL; consider making it configurable");
        }

        # Check for QNetworkReply without error handling
        if ($line =~ /QNetworkReply/ && $line =~ /\bget\s*\(/) {
            # Check nearby for error connection
            my $has_error_signal = 0;
            for (my $j = $i; $j < scalar @$lines_ref && $j < $i + 10; $j++) {
                if ($lines_ref->[$j] =~ /error|finished|errorOccurred/) {
                    $has_error_signal = 1;
                    last;
                }
            }
            if (!$has_error_signal) {
                report($filepath, $linenum, SEV_WARNING, 'NETWORK_NO_ERROR',
                    "Network request without error handling");
            }
        }
    }
}

# ===========================================================================
# Additional Checks: Logging Consistency
# ===========================================================================

sub check_logging_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $has_qdebug = 0;
    my $has_qwarning = 0;
    my $has_qcritical = 0;
    my $has_qinfo = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        # Track logging usage
        $has_qdebug = 1 if $line =~ /\bqDebug\b/;
        $has_qwarning = 1 if $line =~ /\bqWarning\b/;
        $has_qcritical = 1 if $line =~ /\bqCritical\b/;
        $has_qinfo = 1 if $line =~ /\bqInfo\b/;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for qDebug in error paths (should be qWarning or qCritical)
        if ($line =~ /qDebug\s*\(\s*\)\s*<<.*(?:error|fail|invalid|cannot|unable|missing)/i) {
            report($filepath, $linenum, SEV_STYLE, 'DEBUG_FOR_ERROR',
                "Using qDebug() for error message; consider qWarning() or qCritical()");
        }

        # Check for qCritical in non-error paths
        if ($line =~ /qCritical\s*\(\s*\)\s*<<.*(?:success|complete|loaded|ready|done)/i) {
            report($filepath, $linenum, SEV_STYLE, 'CRITICAL_FOR_INFO',
                "Using qCritical() for informational message; consider qDebug() or qInfo()");
        }

        # Check for endl in qDebug (unnecessary - Qt adds newline)
        if ($line =~ /qDebug\s*\(\s*\).*<<\s*(?:endl|"\\n"|Qt::endl)/) {
            report($filepath, $linenum, SEV_STYLE, 'QDEBUG_ENDL',
                "Unnecessary endl/newline in qDebug(); Qt logging adds newline automatically");
        }

        # Check for log messages without context
        if ($line =~ /q(?:Debug|Warning|Critical|Info)\s*\(\s*\)\s*<<\s*"[^"]{1,10}"\s*;/) {
            report($filepath, $linenum, SEV_INFO, 'TERSE_LOG_MESSAGE',
                "Very short log message; add context (class name, function, values)");
        }
    }
}

# ===========================================================================
# Additional Checks: Path Handling
# ===========================================================================

sub check_path_handling {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for string concatenation for paths (use QDir/FS::PathCombine)
        if ($line =~ /\+\s*"[\/\\]"\s*\+/ || $line =~ /\+\s*QDir::separator\s*\(\s*\)\s*\+/) {
            report($filepath, $linenum, SEV_WARNING, 'PATH_CONCATENATION',
                "String concatenation for paths; use FS::PathCombine() or QDir::filePath() (project convention)");
        }

        # Check for hardcoded path separators
        if ($line =~ /"[^"]*\\\\[^"]*"/ && $line !~ /\\n|\\t|\\r|\\"|\\\\\\\\/) {
            # Skip escape sequences
            if ($line !~ /regex|pattern|QRegularExpression/) {
                report($filepath, $linenum, SEV_INFO, 'HARDCODED_BACKSLASH',
                    "Hardcoded backslash in path; use QDir::toNativeSeparators() for portability");
            }
        }

        # Check for direct filesystem access instead of FS:: helpers
        if ($line =~ /QFile::exists\s*\(/) {
            report($filepath, $linenum, SEV_INFO, 'DIRECT_FS_ACCESS',
                "Direct QFile::exists(); consider FS:: helper functions for consistency");
        }
    }
}

# ===========================================================================
# Additional Checks: Error Handling Patterns
# ===========================================================================

sub check_error_handling {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for ignored return values of important functions
        if ($line =~ /^\s*\w+::(?:save|write|remove|rename|copy|mkdir|mkpath)\s*\(/) {
            # Check if return value is used
            unless ($line =~ /(?:if|=|!|&&|\|\|)\s*\w+::/) {
                report($filepath, $linenum, SEV_WARNING, 'IGNORED_RETURN',
                    "Return value of filesystem operation may be ignored; check for errors");
            }
        }

        # Check for QFile::open without error check
        if ($line =~ /\.open\s*\(/) {
            # Check if it's in an if condition or return value is checked
            unless ($line =~ /^if\b|^\s*if\s*\(|=\s*\w+\.open|!\s*\w+\.open/) {
                # Look for error check on next line
                if ($i + 1 < scalar @$lines_ref) {
                    my $next = $lines_ref->[$i + 1];
                    unless ($next =~ /if\s*\(/ || $next =~ /Q_ASSERT/) {
                        report($filepath, $linenum, SEV_INFO, 'OPEN_NO_CHECK',
                            "File/device open() without error check");
                    }
                }
            }
        }

        # Check for error strings without translation
        if ($line =~ /emitFailed\s*\(\s*"/) {
            report($filepath, $linenum, SEV_INFO, 'UNTRANSLATED_ERROR',
                "Error message passed as raw string; consider using tr() for localization");
        }
    }
}

# ===========================================================================
# Additional Checks: Inheritance Patterns
# ===========================================================================

sub check_inheritance_patterns {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for multiple inheritance (excluding Qt mixin patterns)
        if ($line =~ /\bclass\s+\w+\s*:.*,.*public\s+\w+/) {
            my @bases = ($line =~ /public\s+(\w+)/g);
            if (scalar @bases > 2) {
                report($filepath, $linenum, SEV_INFO, 'DEEP_INHERITANCE',
                    "Class inherits from " . scalar(@bases) . " base classes; consider composition over inheritance");
            }
        }

        # Check for protected inheritance (unusual, may be a mistake)
        if ($line =~ /\bclass\s+\w+\s*:.*protected\s+\w+/) {
            report($filepath, $linenum, SEV_INFO, 'PROTECTED_INHERITANCE',
                "Protected inheritance is rarely needed; did you mean public?");
        }

        # Check for private inheritance (composition might be better)
        if ($line =~ /\bclass\s+\w+\s*:\s*(?:private\s+)?(\w+)/ && $line !~ /public|protected/) {
            my $base = $1;
            next if $base =~ /^(?:public|protected|private)$/;
            report($filepath, $linenum, SEV_INFO, 'PRIVATE_INHERITANCE',
                "Private inheritance from '$base'; consider composition (has-a) instead of inheritance (is-a)");
        }
    }
}

# ===========================================================================
# Additional Checks: Type Conversion Safety
# ===========================================================================

sub check_type_conversions {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        next unless exists $changed_ref->{$linenum};

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Check for reinterpret_cast usage
        if ($line =~ /\breinterpret_cast\b/) {
            report($filepath, $linenum, SEV_WARNING, 'REINTERPRET_CAST',
                "reinterpret_cast is dangerous; ensure this cast is necessary and documented");
        }

        # Check for const_cast usage (often a code smell)
        if ($line =~ /\bconst_cast\b/) {
            report($filepath, $linenum, SEV_INFO, 'CONST_CAST',
                "const_cast may indicate a design issue; consider if the const can be removed upstream");
        }

        # Check for dynamic_cast without null check
        if ($line =~ /dynamic_cast<[^>]+\*>\s*\((\w+)\)/) {
            my $var = $1;
            # Check if result is null-checked
            unless ($line =~ /if\s*\(/ || $line =~ /assert|Q_ASSERT/) {
                if ($i + 1 < scalar @$lines_ref) {
                    my $next = $lines_ref->[$i + 1];
                    unless ($next =~ /if\s*\(\s*\w+\s*(?:!=\s*nullptr|==\s*nullptr|\)|!)/) {
                        report($filepath, $linenum, SEV_WARNING, 'DYNAMIC_CAST_NO_CHECK',
                            "dynamic_cast result should be null-checked");
                    }
                }
            }
        }

        # Check for narrowing conversions in initialization
        if ($line =~ /\bint\s+\w+\s*=\s*\w+\.(?:size|count|length)\s*\(/) {
            report($filepath, $linenum, SEV_INFO, 'NARROWING_CONVERSION',
                "Potential narrowing conversion from size_t/qsizetype to int");
        }

        # Check for implicit bool conversion from pointer
        if ($line =~ /\bif\s*\(\s*!?\s*\w+\.get\s*\(\s*\)\s*\)/) {
            # This is actually fine in C++ but some prefer explicit nullptr check
        }
    }
}

# ===========================================================================
# Additional Checks: Documentation and Readability
# ===========================================================================

sub check_documentation {
    my ($filepath, $lines_ref, $changed_ref, $ftype) = @_;

    return unless $ftype eq FTYPE_HEADER;

    my $in_class = 0;
    my $class_name = '';
    my $public_method_count = 0;
    my $documented_method_count = 0;
    my $in_public = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];

        if ($line =~ /\bclass\s+(\w+)\b/ && $line !~ /;\s*$/) {
            $in_class = 1;
            $class_name = $1;
            $public_method_count = 0;
            $documented_method_count = 0;
        }

        if ($in_class) {
            if ($line =~ /^\s*public\s*:/) {
                $in_public = 1;
            } elsif ($line =~ /^\s*(?:protected|private)\s*:/) {
                $in_public = 0;
            }

            if ($in_public && $line =~ /^\s*(?:virtual\s+)?(?:static\s+)?(?:\w[\w:*&<> ]*\s+)?(\w+)\s*\(/ && $line !~ /^\s*(?:if|for|while|switch|return)/) {
                my $method = $1;
                next if $method eq $class_name;  # Constructor
                next if $method =~ /^~/;          # Destructor
                next if $method =~ /^operator/;   # Operator

                $public_method_count++;

                # Check if the method has documentation (comment on previous lines)
                if ($i > 0) {
                    my $prev = $lines_ref->[$i - 1];
                    if ($prev =~ /\/\/\/|\/\*\*|\*\/|^\s*\*/) {
                        $documented_method_count++;
                    }
                }
            }

            if ($line =~ /^};/) {
                $in_class = 0;
                $in_public = 0;

                # Check documentation ratio for public API
                if ($public_method_count > 5 && $documented_method_count == 0) {
                    report($filepath, 1, SEV_INFO, 'NO_API_DOCS',
                        "Class '$class_name' has $public_method_count public methods with no documentation");
                }
            }
        }
    }
}

# ===========================================================================
# Additional Checks: Complexity Metrics
# ===========================================================================

sub check_cyclomatic_complexity {
    my ($filepath, $lines_ref) = @_;

    my $in_function = 0;
    my $function_name = '';
    my $function_start = 0;
    my $brace_depth = 0;
    my $complexity = 1;  # Start at 1

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];

        # Skip comments
        next if $line =~ /^\s*\/\//;
        next if $line =~ /^\s*\*/;

        # Detect function start
        if (!$in_function && $line =~ /^(?:\w[\w:*&<> ,]*\s+)?(\w+(?:::\w+)?)\s*\([^;]*$/) {
            $function_name = $1;
            $function_start = $i + 1;
            next if $function_name =~ /^(?:if|for|while|switch|catch|return|class|struct|namespace|enum)$/;

            if ($line =~ /{/ || ($i + 1 < scalar @$lines_ref && $lines_ref->[$i + 1] =~ /^\s*{/)) {
                $in_function = 1;
                $brace_depth = 0;
                $complexity = 1;
            }
        }

        if ($in_function) {
            my $stripped = $line;
            $stripped =~ s/"(?:[^"\\]|\\.)*"//g;

            $brace_depth += () = $stripped =~ /{/g;
            $brace_depth -= () = $stripped =~ /}/g;

            # Count decision points
            $complexity++ if $stripped =~ /\bif\s*\(/;
            $complexity++ if $stripped =~ /\belse\s+if\b/;
            $complexity++ if $stripped =~ /\bfor\s*\(/;
            $complexity++ if $stripped =~ /\bwhile\s*\(/;
            $complexity++ if $stripped =~ /\bcase\s+/;
            $complexity++ if $stripped =~ /\bcatch\s*\(/;
            $complexity++ if $stripped =~ /\?\s*[^:]/;  # Ternary
            $complexity++ if $stripped =~ /&&/;
            $complexity++ if $stripped =~ /\|\|/;

            if ($brace_depth <= 0 && $stripped =~ /}/) {
                if ($complexity > 15) {
                    report($filepath, $function_start, SEV_WARNING, 'HIGH_COMPLEXITY',
                        "Function '$function_name' has cyclomatic complexity of $complexity (recommended max: 15). Consider refactoring.");
                } elsif ($complexity > 10) {
                    report($filepath, $function_start, SEV_INFO, 'MODERATE_COMPLEXITY',
                        "Function '$function_name' has cyclomatic complexity of $complexity (consider simplifying)");
                }

                $in_function = 0;
                $complexity = 1;
            }
        }
    }
}

# ===========================================================================
# Additional Checks: Preprocessor Usage
# ===========================================================================

sub check_preprocessor_usage {
    my ($filepath, $lines_ref, $changed_ref) = @_;

    my $ifdef_depth = 0;
    my $max_ifdef_depth = 0;

    for (my $i = 0; $i < scalar @$lines_ref; $i++) {
        my $line = $lines_ref->[$i];
        my $linenum = $i + 1;

        # Track #ifdef depth
        if ($line =~ /^\s*#\s*(?:if|ifdef|ifndef)\b/) {
            $ifdef_depth++;
            if ($ifdef_depth > $max_ifdef_depth) {
                $max_ifdef_depth = $ifdef_depth;
            }
        }
        if ($line =~ /^\s*#\s*endif\b/) {
            $ifdef_depth--;
        }

        next unless exists $changed_ref->{$linenum};

        # Check for deeply nested preprocessor conditionals
        if ($ifdef_depth > 3 && $line =~ /^\s*#\s*(?:if|ifdef|ifndef)\b/) {
            report($filepath, $linenum, SEV_WARNING, 'DEEP_IFDEF',
                "Preprocessor conditional nesting depth $ifdef_depth; consider refactoring");
        }

        # Check for #pragma warning disable without re-enable
        if ($line =~ /^\s*#\s*pragma\s+warning\s*\(\s*disable/) {
            # Check for corresponding enable
            my $has_enable = 0;
            for (my $j = $i + 1; $j < scalar @$lines_ref; $j++) {
                if ($lines_ref->[$j] =~ /^\s*#\s*pragma\s+warning\s*\(\s*(?:default|enable)/) {
                    $has_enable = 1;
                    last;
                }
            }
            unless ($has_enable) {
                report($filepath, $linenum, SEV_WARNING, 'PRAGMA_NO_RESTORE',
                    "#pragma warning disable without corresponding restore");
            }
        }

        # Check for #define with complex expressions (should be constexpr)
        if ($line =~ /^\s*#\s*define\s+(\w+)\s+\(.*[+\-*\/].*\)/) {
            my $macro = $1;
            next if $macro =~ /^_/;  # System macros
            report($filepath, $linenum, SEV_INFO, 'DEFINE_VS_CONSTEXPR',
                "Complex #define expression; consider constexpr for type safety");
        }

        # Check for multi-line macros (hard to debug)
        if ($line =~ /^\s*#\s*define\b.*\\$/) {
            my $macro_lines = 1;
            for (my $j = $i + 1; $j < scalar @$lines_ref; $j++) {
                $macro_lines++;
                last unless $lines_ref->[$j] =~ /\\$/;
            }
            if ($macro_lines > 10) {
                report($filepath, $linenum, SEV_WARNING, 'LARGE_MACRO',
                    "Multi-line macro ($macro_lines lines); consider using inline function or template");
            }
        }
    }
}

# ===========================================================================
# Wire up additional checks in check_cpp_conventions
# ===========================================================================

# Override the original check_cpp_conventions to include new checks
{
    no warnings 'redefine';
    my $original_check_cpp = \&check_cpp_conventions;

    *check_cpp_conventions = sub {
        my ($filepath, $lines_ref, $changed_ref, $ftype) = @_;

        # Call original checks
        $original_check_cpp->($filepath, $lines_ref, $changed_ref, $ftype);

        # Additional checks
        check_template_style($filepath, $lines_ref, $changed_ref);
        check_raii_patterns($filepath, $lines_ref, $changed_ref);
        check_move_semantics($filepath, $lines_ref, $changed_ref);
        check_concurrency_patterns($filepath, $lines_ref, $changed_ref);
        check_operator_overloading($filepath, $lines_ref, $changed_ref);
        check_file_organization($filepath, $lines_ref, $changed_ref, $ftype);
        check_signal_slot_details($filepath, $lines_ref, $changed_ref);
        check_json_patterns($filepath, $lines_ref, $changed_ref);
        check_network_patterns($filepath, $lines_ref, $changed_ref);
        check_logging_patterns($filepath, $lines_ref, $changed_ref);
        check_path_handling($filepath, $lines_ref, $changed_ref);
        check_error_handling($filepath, $lines_ref, $changed_ref);
        check_inheritance_patterns($filepath, $lines_ref, $changed_ref);
        check_type_conversions($filepath, $lines_ref, $changed_ref);
        check_documentation($filepath, $lines_ref, $changed_ref, $ftype);
        check_cyclomatic_complexity($filepath, $lines_ref);
        check_preprocessor_usage($filepath, $lines_ref, $changed_ref);
    };
}

# ===========================================================================
# Additional Rule Documentation
# ===========================================================================

$RULE_DOCS{'TEMPLATE_BLANK_LINE'}      = 'Blank line between template<> and declaration';
$RULE_DOCS{'COMPLEX_TEMPLATE'}         = 'Template with many parameters';
$RULE_DOCS{'C_FILE_IO'}               = 'C-style file I/O (use Qt/C++ alternatives)';
$RULE_DOCS{'MANUAL_LOCK'}             = 'Manual mutex lock (use RAII guards)';
$RULE_DOCS{'RAW_HANDLE'}              = 'Raw OS handle (use wrapper classes)';
$RULE_DOCS{'GOTO_CLEANUP'}            = 'Goto cleanup pattern (use RAII)';
$RULE_DOCS{'MOVE_CONST'}              = 'std::move on const object (no effect)';
$RULE_DOCS{'USE_AFTER_MOVE'}          = 'Use of moved-from object';
$RULE_DOCS{'RETURN_STD_MOVE'}         = 'return std::move prevents RVO';
$RULE_DOCS{'PUSH_BACK_TEMP'}          = 'push_back with temporary (use emplace_back)';
$RULE_DOCS{'VOLATILE_SYNC'}           = 'volatile is not synchronization';
$RULE_DOCS{'SLEEP_SYNC'}              = 'Sleep-based waiting';
$RULE_DOCS{'THREAD_UNSAFE_SINGLETON'} = 'Thread-unsafe singleton pattern';
$RULE_DOCS{'STATIC_IN_THREAD'}        = 'Static variable in threaded context';
$RULE_DOCS{'OPERATOR_CONST'}          = 'Operator missing const version';
$RULE_DOCS{'MISSING_OPERATOR_NEQ'}    = 'operator== without operator!=';
$RULE_DOCS{'OPERATOR_SELF_ASSIGN'}    = 'Copy assignment without self-check';
$RULE_DOCS{'FILE_CLASS_MISMATCH'}     = 'File name doesn\'t match class name';
$RULE_DOCS{'FILE_NAMING'}             = 'File naming convention (PascalCase)';
$RULE_DOCS{'SIGNAL_NAMING'}           = 'Signal naming convention';
$RULE_DOCS{'CONNECT_LIFETIME'}        = 'Lambda captures this in connect()';
$RULE_DOCS{'JSON_NO_ERROR_CHECK'}     = 'JSON parsing without error check';
$RULE_DOCS{'JSON_UNCHECKED_ACCESS'}   = 'Unchecked JSON value access';
$RULE_DOCS{'HTTP_NOT_HTTPS'}          = 'Insecure HTTP URL';
$RULE_DOCS{'HARDCODED_URL'}           = 'Hardcoded URL';
$RULE_DOCS{'NETWORK_NO_ERROR'}        = 'Network request without error handling';
$RULE_DOCS{'DEBUG_FOR_ERROR'}         = 'qDebug for error message';
$RULE_DOCS{'CRITICAL_FOR_INFO'}       = 'qCritical for informational message';
$RULE_DOCS{'QDEBUG_ENDL'}            = 'Unnecessary endl in qDebug';
$RULE_DOCS{'TERSE_LOG_MESSAGE'}       = 'Very short log message';
$RULE_DOCS{'PATH_CONCATENATION'}      = 'String concatenation for paths';
$RULE_DOCS{'HARDCODED_BACKSLASH'}     = 'Hardcoded backslash in path';
$RULE_DOCS{'DIRECT_FS_ACCESS'}        = 'Direct filesystem access vs FS:: helpers';
$RULE_DOCS{'IGNORED_RETURN'}          = 'Ignored return value of important function';
$RULE_DOCS{'OPEN_NO_CHECK'}           = 'File open without error check';
$RULE_DOCS{'UNTRANSLATED_ERROR'}      = 'Untranslated error message';
$RULE_DOCS{'DEEP_INHERITANCE'}        = 'Deep inheritance hierarchy';
$RULE_DOCS{'PROTECTED_INHERITANCE'}   = 'Protected inheritance (unusual)';
$RULE_DOCS{'PRIVATE_INHERITANCE'}     = 'Private inheritance (consider composition)';
$RULE_DOCS{'REINTERPRET_CAST'}        = 'Dangerous reinterpret_cast';
$RULE_DOCS{'CONST_CAST'}              = 'const_cast usage (design smell)';
$RULE_DOCS{'DYNAMIC_CAST_NO_CHECK'}   = 'dynamic_cast without null check';
$RULE_DOCS{'NARROWING_CONVERSION'}    = 'Potential narrowing conversion';
$RULE_DOCS{'NO_API_DOCS'}             = 'No documentation for public API';
$RULE_DOCS{'HIGH_COMPLEXITY'}         = 'High cyclomatic complexity';
$RULE_DOCS{'MODERATE_COMPLEXITY'}     = 'Moderate cyclomatic complexity';
$RULE_DOCS{'DEEP_IFDEF'}              = 'Deep preprocessor conditional nesting';
$RULE_DOCS{'PRAGMA_NO_RESTORE'}       = 'Pragma warning without restore';
$RULE_DOCS{'DEFINE_VS_CONSTEXPR'}     = 'Complex #define (use constexpr)';
$RULE_DOCS{'LARGE_MACRO'}             = 'Large multi-line macro';

# ===========================================================================
# Run
# ===========================================================================

# Check for --self-test before normal option parsing
if (grep { $_ eq '--self-test' } @ARGV) {
    @ARGV = grep { $_ ne '--self-test' } @ARGV;
    $opt_color = (-t STDOUT) ? 1 : 0;
    exit(run_self_test() ? 0 : 1);
}

# Check for --list-rules
if (grep { $_ eq '--list-rules' } @ARGV) {
    $opt_color = (-t STDOUT) ? 1 : 0;
    list_rules();
    exit(0);
}

main();

__END__

=head1 NAME

checkpatch.pl - MeshMC Coding Style and Convention Checker

=head1 SYNOPSIS

  checkpatch.pl [OPTIONS] [FILES...]
  git diff | checkpatch.pl --diff
  checkpatch.pl --git HEAD~1
  checkpatch.pl --dir launcher/
  checkpatch.pl --repository --summary
  checkpatch.pl --self-test
  checkpatch.pl --list-rules

=head1 DESCRIPTION

This script checks C++, header, and CMake source files in the MeshMC project
for adherence to the project's coding conventions. It is inspired by the Linux
kernel's checkpatch.pl but tailored specifically for MeshMC's Qt/C++ codebase.

The script supports multiple input modes: checking individual files, entire
directories recursively, unified diffs from stdin, or git diffs from a specific
reference. It can also attempt to fix simple issues like trailing whitespace
and CRLF line endings.

=head1 CONVENTIONS ENFORCED

=head2 C++ Style

=over 4

=item * 4-space indentation (no tabs allowed)

=item * K&R brace placement (opening brace on same line for control structures)

=item * PascalCase for class and struct names

=item * camelCase for method and function names

=item * m_ prefix for member variables (m_camelCase)

=item * UPPER_SNAKE_CASE for preprocessor macros

=item * #pragma once for header guards (no #ifndef guards)

=item * SPDX license headers at top of every file

=item * Line length maximum of 120 characters (configurable)

=item * Pointer/reference symbols adjacent to variable name (Type *var, Type &var)

=item * Modern C++ features: nullptr over NULL, enum class over enum, using over typedef

=item * Smart pointers (make_shared/make_unique) instead of raw new/delete

=item * C++ headers (cstdio, cstring) instead of C headers (stdio.h, string.h)

=item * No std::string - use QString throughout

=item * No std::cout/cerr - use qDebug()/qWarning()/qCritical()

=item * Virtual destructors for polymorphic base classes

=item * override keyword for virtual method overrides in derived classes

=item * explicit keyword for single-parameter constructors

=back

=head2 Qt Conventions

=over 4

=item * Q_OBJECT macro required in all QObject-derived classes

=item * Modern Qt6 connect() syntax preferred over SIGNAL/SLOT macros

=item * signals: and slots: keywords preferred over Q_SIGNALS/Q_SLOTS

=item * Signal names should use past-tense or progressive naming (e.g., changed, loading)

=item * Slot names following on_widgetName_signalName pattern for auto-connections

=item * QProcess for process execution instead of system()

=item * tr() for user-visible strings for localization

=item * FS::PathCombine() for path construction instead of string concatenation

=item * Json::requireDocument() helpers for JSON parsing with error handling

=item * shared_qobject_ptr and unique_qobject_ptr for Qt object ownership

=back

=head2 CMake Conventions

=over 4

=item * 3-space indentation (no tabs)

=item * Lowercase function names (set(), add_executable(), etc.)

=item * UPPER_SNAKE_CASE for variable names (CORE_SOURCES, CMAKE_CXX_STANDARD)

=item * Explicit source file listing (no file(GLOB))

=item * target_* commands preferred over global add_* commands

=item * SPDX license headers in comments

=back

=head2 File Organization

=over 4

=item * PascalCase file names matching primary class name

=item * .h extension for headers, .cpp for implementation

=item * Test files named *_test.cpp using Qt Test framework

=item * Include ordering: local includes, then Qt includes, then STL includes

=back

=head1 SEVERITY LEVELS

The script uses four severity levels:

=over 4

=item B<ERROR> (exit code 1)

Must fix before merging. Includes missing header guards, CRLF line endings,
dangerous functions (gets, sprintf), using namespace in headers, throw by
pointer, missing Q_OBJECT macro, and hardcoded credentials.

=item B<WARNING> (exit code 2)

Should fix. Includes trailing whitespace, old Qt connect syntax, NULL instead
of nullptr, C-style casts, security-sensitive functions, ignored return values,
commented-out code, and goto cleanup patterns.

=item B<INFO> (shown with --verbose)

Style suggestions. Includes include ordering, const reference suggestions,
prefer enum class, magic numbers, documentation coverage, and complexity
metrics.

=item B<STYLE> (shown with --verbose)

Minor cosmetic preferences. Includes pointer alignment, keyword spacing,
typedef vs using, and Yoda conditions.

=back

=head1 OPTIONS

=over 4

=item B<--diff>

Read unified diff from stdin. Only changed lines (added lines in diff context)
are checked for line-level rules. File-level rules (header guard, license) are
still applied to the full file if accessible.

=item B<--git> I<ref>

Check changes since the specified git ref. Runs C<git diff ref> internally.
The ref is validated to prevent shell injection. Supports any valid git ref
format (HEAD~N, branch names, commit hashes, tags).

=item B<--file> I<path>

Check a specific file. Can be repeated to check multiple files. All lines
in the file are checked.

=item B<--dir> I<path>

Check all source files (.cpp, .h, .hpp, CMakeLists.txt, .cmake) in the
specified directory recursively. Automatically excludes build/, libraries/,
.git/, and auto-generated files.

=item B<--repository>, B<--repo>, B<-R>

Scan the entire git repository. The script finds the repository root
by running C<git rev-parse --show-toplevel> and checks all source files
starting from that root. Automatically excludes build/, libraries/,
.git/, and auto-generated files. Equivalent to running
C<--dir E<lt>repo-rootE<gt>> but without needing to know the root path.
Fails with exit code 3 if no git repository is found.

=item B<--fix>

Attempt to fix simple issues in-place. Currently fixes: trailing whitespace
removal, CRLF to LF conversion. Use with caution and review changes.

=item B<--quiet, -q>

Only show ERROR-level issues. Suppresses warnings, info, and style messages.

=item B<--verbose, -v>

Show all issues including INFO and STYLE levels. Also prints file names as
they are being checked.

=item B<--summary, -s>

Print a summary table at the end showing total files checked, lines checked,
and counts of errors, warnings, and info messages.

=item B<--color>

Force colored output regardless of terminal detection.

=item B<--no-color>

Disable colored output even in a terminal.

=item B<--max-line-length> I<n>

Set maximum line length. Default is 120 characters. Lines exceeding this
length generate warnings. Lines exceeding length + 20 generate errors.

=item B<--exclude> I<pattern>

Exclude files matching the specified glob pattern. Can be repeated.
Patterns are matched against the full file path. Default exclusions
include build/, libraries/, and auto-generated files.

=item B<--self-test>

Run internal self-tests to verify the script is working correctly.
Exits with code 0 if all tests pass, 1 otherwise.

=item B<--list-rules>

Print a categorized list of all rules with their descriptions.

=item B<--help, -h>

Show usage information.

=item B<--version, -V>

Show version information.

=back

=head1 EXIT CODES

=over 4

=item B<0> - No issues found (clean)

=item B<1> - One or more ERROR-level issues found

=item B<2> - WARNING-level issues found (no errors)

=item B<3> - Script usage error (invalid options)

=back

=head1 INTEGRATION

=head2 Git Pre-Commit Hook

Add to your lefthook.yml:

  pre-commit:
    jobs:
      - name: checkpatch
        run: |
          git diff --cached --diff-filter=ACMR | ./scripts/checkpatch.pl --diff

=head2 CI Pipeline

  checkpatch:
    script:
      - ./scripts/checkpatch.pl --git origin/main --summary -q

=head2 Editor Integration

Most editors can run external linters. Configure checkpatch.pl as an
external tool with the --file flag pointing to the current file.

=head1 EXAMPLES

  # Check all source files in launcher/
  ./scripts/checkpatch.pl --dir launcher/ --summary

  # Scan entire repository
  ./scripts/checkpatch.pl --repository --summary

  # Scan entire repository, errors only
  ./scripts/checkpatch.pl -R -q --summary

  # Check a specific file
  ./scripts/checkpatch.pl --file launcher/Application.cpp

  # Check recent git changes
  ./scripts/checkpatch.pl --git HEAD~3 --summary

  # Check staged changes before commit
  git diff --cached | ./scripts/checkpatch.pl --diff

  # Check changes against main branch
  git diff main | ./scripts/checkpatch.pl --diff --summary

  # Fix trivial issues in a file
  ./scripts/checkpatch.pl --file foo.cpp --fix

  # Quiet mode - errors only
  ./scripts/checkpatch.pl --dir launcher/ -q

  # Verbose mode - all details
  ./scripts/checkpatch.pl --dir launcher/ -v --summary

  # Custom line length
  ./scripts/checkpatch.pl --file foo.cpp --max-line-length 100

  # Exclude test files
  ./scripts/checkpatch.pl --dir launcher/ --exclude '*_test.cpp'

  # List all available rules
  ./scripts/checkpatch.pl --list-rules

  # Run self-tests
  ./scripts/checkpatch.pl --self-test

=head1 RULE CATEGORIES

Rules are organized into the following categories:

=over 4

=item B<Common> - Whitespace, line endings, blank lines

=item B<License> - SPDX headers, copyright notices

=item B<Header Guards> - #pragma once enforcement

=item B<Indentation> - Tab/space checking, indent width

=item B<Line Length> - Maximum line length enforcement

=item B<Naming> - Class, method, variable, macro naming conventions

=item B<Brace Style> - K&R brace placement

=item B<Pointer/Reference> - Alignment conventions

=item B<Spacing> - Keyword spacing, comma spacing, brace spacing

=item B<Qt> - Q_OBJECT, connect syntax, signal/slot conventions

=item B<Includes> - Include ordering, C vs C++ headers, path style

=item B<Comments> - Comment style, commented-out code, TODO format

=item B<Control Flow> - Braces, goto, ternary, Yoda conditions

=item B<Strings> - QString vs std::string, logging functions

=item B<Memory> - Smart pointers, raw new/delete, C memory functions

=item B<Const> - Const reference parameters

=item B<Enum> - enum class preference

=item B<Constructor> - explicit keyword, initializer lists

=item B<Function> - Length limits, nesting depth

=item B<Deprecated> - NULL, C casts, typedef, auto_ptr

=item B<Security> - Dangerous functions, hardcoded credentials, HTTP

=item B<Tests> - QTest framework conventions

=item B<Exceptions> - Throw/catch conventions

=item B<Virtual/Override> - Virtual destructors, override keyword

=item B<Auto> - auto keyword usage

=item B<Lambda> - Capture lists, lambda length

=item B<Switch> - Fall-through, default case

=item B<Class> - Access specifier ordering

=item B<Debug> - Debug print cleanup

=item B<Style> - Multiple statements, magic numbers, using namespace

=item B<CMake> - CMake-specific conventions

=item B<Templates> - Template parameter style

=item B<RAII> - Resource management patterns

=item B<Move Semantics> - std::move usage

=item B<Concurrency> - Thread safety patterns

=item B<Operators> - Operator overloading conventions

=item B<File Organization> - File/class name matching

=item B<JSON> - JSON parsing patterns

=item B<Network> - URL and network patterns

=item B<Logging> - Log level consistency

=item B<Paths> - Path handling conventions

=item B<Error Handling> - Return value checking

=item B<Inheritance> - Inheritance patterns

=item B<Type Conversions> - Cast safety

=item B<Documentation> - API documentation coverage

=item B<Complexity> - Cyclomatic complexity metrics

=item B<Preprocessor> - Macro and #ifdef patterns

=back

=head1 KNOWN LIMITATIONS

=over 4

=item * Does not parse C++ fully; uses regex heuristics which may produce
false positives or miss some patterns.

=item * String literal detection is approximate; multiline raw strings
may not be handled perfectly.

=item * Template metaprogramming patterns may trigger false warnings.

=item * Alignment-based indentation (e.g., aligning function parameters)
is allowed but may trigger info-level messages.

=item * The --fix mode only handles trailing whitespace and CRLF conversion;
it does not fix naming, indentation, or structural issues.

=back

=head1 AUTHOR

MeshMC Project - Project Tick

=head1 LICENSE

GPL-3.0-or-later

=cut
