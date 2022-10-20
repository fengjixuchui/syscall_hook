#include "hook.h"
static int replace_call_func(unsigned long handler, unsigned long orig_func, unsigned long custom_func)
{
  unsigned char *tmp_addr = (unsigned char*)handler;
  int i = 0;
  do{
    /* in x86_64 the call instruction opcode is 0x8e, occupy 1+4 bytes(E8+offset) totally*/
    if(*tmp_addr == 0xe8){ 
      int* offset = (int*)(tmp_addr+1);
      if(((unsigned long)tmp_addr + 5 + *offset) == orig_func){
        /* replace with my_func relative addr(offset) */
        *offset=custom_func-(unsigned long)tmp_addr-5;
        pr_info("base addr: 0x%08lx, offset:%08lx, replace func: %08lx to func: %08lx.\n", (unsigned long)tmp_addr, (unsigned long)(*offset), orig_func, custom_func);
        return 0;
      }
    }
    tmp_addr++;
  }while(i++ < 128);
  return 1;
}

inline int install_hook(syscall_fn_t *sys_call_table, struct syscall_hook *hook)
{
    int res;
    char fname_lookup[NAME_MAX];
    unsigned long syscall_base_addr=0;
    syscall_base_addr =  (unsigned long)sys_call_table[hook->syscall_nr];
    sprint_symbol(fname_lookup, syscall_base_addr);
    if(strncmp(fname_lookup, "stub", 4)== 0)
    {
        res = set_addr_rw(syscall_base_addr);
        if (res != 0) {
            pr_err("set sys_call_table writeable failed: %d\n", res);
            return res;
        }
        replace_call_func((unsigned long)syscall_base_addr, lookup_name(hook->name), (unsigned long)hook->custom_syscall);
        res = set_addr_ro(syscall_base_addr);
        if (res != 0) {
            pr_err("set sys_call_table read only failed: %d\n", res);
            return res;
        }
        *hook->org_syscall = (syscall_fn_t)lookup_name(hook->name);
    }
    else{
        *hook->org_syscall = sys_call_table[hook->syscall_nr];
        pr_info("org_fn addr: %lx\n", (unsigned long)*hook->org_syscall);
        res = set_addr_rw((unsigned long)(sys_call_table + hook->syscall_nr));
        if (res != 0) {
            pr_err("set sys_call_table writeable failed: %d\n", res);
            return res;
        }
        sys_call_table[hook->syscall_nr] = hook->custom_syscall;
        res = set_addr_ro((unsigned long)(sys_call_table + hook->syscall_nr));
        if (res != 0) {
            pr_err("set sys_call_table read only failed: %d\n", res);
            return res;
        }
    }

    return 0;
}

inline int uninstall_hook(syscall_fn_t *sys_call_table, struct syscall_hook *hook)
{
    int res = 0;
    char fname_lookup[NAME_MAX];
    unsigned long syscall_base_addr = 0;
    if (*hook->org_syscall == (syscall_fn_t)NULL) return 0;

    syscall_base_addr =  (unsigned long)sys_call_table[hook->syscall_nr];
    sprint_symbol(fname_lookup, syscall_base_addr);
    if(strncmp(fname_lookup, "stub", 4)== 0)
    {
        res = set_addr_rw(syscall_base_addr);
        if (res != 0) {
            pr_err("set sys_call_table writeable failed: %d\n", res);
            return -EFAULT;
        }
        replace_call_func((unsigned long)syscall_base_addr, (unsigned long)hook->custom_syscall, lookup_name(hook->name));
        res = set_addr_ro(syscall_base_addr);
        if (res != 0)
        {
            pr_err("set sys_call_table read only failed: %d\n", res);
            return res;
        }
    }
    else{
        res = set_addr_rw((unsigned long)(sys_call_table + hook->syscall_nr));
        if (res != 0) {
            pr_err("set sys_call_table writeable failed: %d\n", res);
            return -EFAULT;
        }
        sys_call_table[hook->syscall_nr] = *hook->org_syscall;
        res = set_addr_ro((unsigned long)(sys_call_table + hook->syscall_nr));
        if (res != 0)
        {
            pr_err("set sys_call_table read only failed: %d\n", res);
            return res;
        }
    }

    return 0;
}

int install_hooks(syscall_fn_t *sys_call_table, struct syscall_hook *hook, int hook_count)
{
    int i, ret;
    for(i=0; i<hook_count; i++)
    {
        pr_info("install hook %s\n", hook[i].name);
        ret = install_hook(sys_call_table, &hook[i]);
        if(ret)
        {
            pr_err("install hooks failed uninstall theme!\n");
            uninstall_hooks(sys_call_table, hook, hook_count);
            return ret;
        }
    }
    return 0;
}

int uninstall_hooks(syscall_fn_t *sys_call_table, struct syscall_hook *hook, int hook_count)
{
    int i, ret;
    for(i=0; i<hook_count; i++)
    {
        pr_info("uninstall hook %s\n", hook[i].name);
        ret = uninstall_hook(sys_call_table, &hook[i]);
        if(ret)
            return ret;
    }
    return 0;
}