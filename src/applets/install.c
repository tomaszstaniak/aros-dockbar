#define _POSIX_C_SOURCE 200809L
#include "install.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define INSTALL_DEPTH 8

static int join(char *out, size_t cap, const char *dir, const char *name) {
  size_t n = strlen(dir);
  int w = snprintf(out, cap, "%s%s%s", dir,
                   n && (dir[n - 1] == ':' || dir[n - 1] == '/') ? "" : "/",
                   name);
  return w >= 0 && (size_t)w < cap;
}
static int exists(const char *p) {
  struct stat st;
  return !lstat(p, &st);
}
/* Only for our own staging and old drawers, which hold a copy of a package:
 * files and drawers, no links (copy_tree refuses those). */
static int remove_tree(const char *p, int depth) {
  struct stat st;
  if (lstat(p, &st))
    return 1;
  if (!S_ISDIR(st.st_mode))
    return !remove(p);
  if (depth > INSTALL_DEPTH)
    return 0;
  DIR *d = opendir(p);
  if (!d)
    return 0;
  int ok = 1;
  struct dirent *e;
  /* Entries are collected first: deleting while reading a drawer skips
   * entries on some file systems. */
  char (*names)[256] = NULL;
  size_t count = 0, cap = 0;
  while ((e = readdir(d))) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
      continue;
    if (count == cap) {
      size_t next = cap ? cap * 2 : 16;
      void *grown = realloc(names, next * sizeof *names);
      if (!grown) {
        ok = 0;
        break;
      }
      names = grown;
      cap = next;
    }
    snprintf(names[count++], sizeof *names, "%s", e->d_name);
  }
  closedir(d);
  /* Paths on the heap: Dockbar runs this on its own (Shell-sized) stack. */
  char *child = count ? malloc(DA_PATH) : NULL;
  if (count && !child)
    ok = 0;
  for (size_t i = 0; ok && i < count; i++)
    ok = join(child, DA_PATH, p, names[i]) && remove_tree(child, depth + 1);
  free(child);
  free(names);
  return ok && !rmdir(p);
}
static int copy_file(const char *from, const char *to, mode_t mode) {
  FILE *in = fopen(from, "rb"), *out = in ? fopen(to, "wb") : NULL;
  char *buf = malloc(8192);
  size_t n;
  int ok = in && out && buf;
  while (ok && (n = fread(buf, 1, 8192, in)) > 0)
    ok = fwrite(buf, 1, n, out) == n;
  free(buf);
  if (in && ferror(in))
    ok = 0;
  if (in)
    fclose(in);
  if (out && fclose(out))
    ok = 0;
  if (ok)
    chmod(to, mode & 0777);
  return ok;
}
static int copy_tree(const char *from, const char *to, int depth) {
  if (depth > INSTALL_DEPTH || mkdir(to, 0755))
    return 0;
  DIR *d = opendir(from);
  if (!d)
    return 0;
  char *a = malloc(DA_PATH), *b = malloc(DA_PATH);
  int ok = a && b;
  struct dirent *e;
  while (ok && (e = readdir(d))) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
      continue;
    struct stat st;
    ok = join(a, DA_PATH, from, e->d_name) && join(b, DA_PATH, to, e->d_name) &&
         !lstat(a, &st);
    if (!ok)
      break;
    if (S_ISLNK(st.st_mode))
      ok = 0;
    else if (S_ISDIR(st.st_mode))
      ok = copy_tree(a, b, depth + 1);
    else if (S_ISREG(st.st_mode))
      ok = copy_file(a, b, st.st_mode);
    else
      ok = 0;
  }
  closedir(d);
  free(a);
  free(b);
  return ok;
}
/* A drawer that cannot be deleted now (a file or the drawer itself is
 * still in use, for example by a program started from it) is moved out of
 * the way under .<id>.stale-N; every install tries to delete those again. */
static int set_aside(const char *root, const char *id, const char *path) {
  char *name = malloc(DA_ID + 32), *to = malloc(DA_PATH);
  int ok = 0;
  for (int n = 1; name && to && n < 100 && !ok; n++) {
    snprintf(name, DA_ID + 32, ".%s.stale-%d", id, n);
    ok = join(to, DA_PATH, root, name) && !exists(to) && !rename(path, to);
  }
  free(name);
  free(to);
  return ok;
}
static void retry_stale(const char *root, const char *id) {
  char *name = malloc(DA_ID + 32), *path = malloc(DA_PATH);
  for (int n = 1; name && path && n < 100; n++) {
    snprintf(name, DA_ID + 32, ".%s.stale-%d", id, n);
    if (join(path, DA_PATH, root, name) && exists(path))
      remove_tree(path, 0);
  }
  free(name);
  free(path);
}
static int clear(const char *root, const char *id, const char *path) {
  return remove_tree(path, 0) || set_aside(root, id, path);
}
static void say(char *text, size_t cap, const char *s) {
  if (text && cap)
    snprintf(text, cap, "%s", s);
}
int dock_applet_install_check(const char *source,
                              const struct DockAppletTarget *t,
                              struct DockAppletPackage *pkg, char *text,
                              size_t cap) {
  struct stat st;
  if (!source || !*source || lstat(source, &st) || !S_ISDIR(st.st_mode)) {
    say(text, cap, "The widget package drawer was not found");
    return DA_INSTALL_REFUSED;
  }
  if (!dock_applet_load_dir(source, t, pkg)) {
    say(text, cap, pkg->reason[0] ? pkg->reason : "Invalid package metadata");
    return DA_INSTALL_REFUSED;
  }
  say(text, cap, pkg->name);
  return DA_INSTALL_OK;
}
static int install_steps(const struct DockAppletPackage *, const char *,
                         const struct DockAppletTarget *,
                         const struct DockAppletPackage *, char *, char *,
                         char *, char *, struct DockAppletPackage *, char *,
                         size_t);
int dock_applet_install_copy(const struct DockAppletPackage *pkg,
                             const char *root,
                             const struct DockAppletTarget *t,
                             const struct DockAppletCatalog *c, char *text,
                             size_t cap) {
  const struct DockAppletPackage *old = NULL;
  int same = 0;
  for (int i = 0; c && i < c->count; i++)
    if (!strcmp(c->packages[i].id, pkg->id)) {
      if (old) {
        say(text, cap,
            "Two installed packages have this ID; remove one, then try again");
        return DA_INSTALL_REFUSED;
      }
      old = &c->packages[i];
    }
  if (old && !strcmp(old->root, pkg->root))
    same = 1;
  if (same) {
    say(text, cap, "This package is the installed one");
    return DA_INSTALL_REFUSED;
  }
  struct work {
    char final[DA_PATH], staging[DA_PATH], aside[DA_PATH], name[DA_ID + 16];
    struct DockAppletPackage check;
  } *w = malloc(sizeof *w);
  if (!w) {
    say(text, cap, "Not enough memory");
    return DA_INSTALL_FAILED;
  }
  int r = install_steps(pkg, root, t, old, w->final, w->staging, w->aside,
                        w->name, &w->check, text, cap);
  free(w);
  return r;
}
static int install_steps(const struct DockAppletPackage *pkg, const char *root,
                         const struct DockAppletTarget *t,
                         const struct DockAppletPackage *old, char *final,
                         char *staging, char *aside, char *name,
                         struct DockAppletPackage *check, char *text,
                         size_t cap) {
  snprintf(name, DA_ID + 16, ".%s.installing", pkg->id);
  if (!join(staging, DA_PATH, root, name))
    goto too_long;
  snprintf(name, DA_ID + 16, ".%s.old", pkg->id);
  if (!join(aside, DA_PATH, root, name))
    goto too_long;
  if (old) {
    if (strlen(old->root) >= DA_PATH)
      goto too_long;
    strcpy(final, old->root);
  } else if (!join(final, DA_PATH, root, pkg->id))
    goto too_long;
  /* An earlier install stopped between its two renames: the old package is
   * only in the aside drawer. Put it back before anything else. */
  if (!exists(final) && exists(aside) && rename(aside, final)) {
    say(text, cap, "An earlier install was not finished and could not be undone");
    return DA_INSTALL_FAILED;
  }
  retry_stale(root, pkg->id);
  if (!clear(root, pkg->id, staging) || !clear(root, pkg->id, aside)) {
    say(text, cap, "Could not remove what an earlier install left behind");
    return DA_INSTALL_FAILED;
  }
  if (!copy_tree(pkg->root, staging, 0)) {
    remove_tree(staging, 0);
    say(text, cap, "Could not copy the package (disk full or write-protected?)");
    return DA_INSTALL_FAILED;
  }
  if (!dock_applet_load_dir(staging, t, check) || strcmp(check->id, pkg->id)) {
    remove_tree(staging, 0);
    say(text, cap, "The copied package does not load; nothing was changed");
    return DA_INSTALL_FAILED;
  }
  int had = exists(final);
  if (had && rename(final, aside)) {
    remove_tree(staging, 0);
    say(text, cap, "The installed package is in use and cannot be replaced now");
    return DA_INSTALL_FAILED;
  }
  if (rename(staging, final)) {
    if (had)
      rename(aside, final);
    remove_tree(staging, 0);
    say(text, cap, "Could not put the new package in place; nothing was changed");
    return DA_INSTALL_FAILED;
  }
  /* The new package is in place; an old copy that cannot be deleted now
   * is retried by the next install. */
  if (had)
    clear(root, pkg->id, aside);
  say(text, cap, pkg->name);
  return had ? DA_INSTALL_UPDATED : DA_INSTALL_OK;
too_long:
  say(text, cap, "Path too long");
  return DA_INSTALL_FAILED;
}
