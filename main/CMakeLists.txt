set(COMPONENT_SRCSDIRS "src")
set(COMPONENT_ADD_INCLUDEDIRS "include")

# Embed the server root certificate into the final binary
set(COMPONENT_EMBED_TXTFILES ${PROJECT_PATH}/server_certs/ca_cert.pem)


register_component()