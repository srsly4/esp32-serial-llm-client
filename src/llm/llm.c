#include "llm.h"
#include <string.h>

#define MAX_PROVIDERS 8

static const llm_provider_t *s_providers[MAX_PROVIDERS];
static int                   s_count = 0;

void llm_register(const llm_provider_t *provider)
{
    if (s_count < MAX_PROVIDERS) {
        s_providers[s_count++] = provider;
    }
}

llm_provider_t *llm_get_provider(const char *name)
{
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_providers[i]->name, name) == 0) {
            return (llm_provider_t *)s_providers[i];
        }
    }
    return NULL;
}
