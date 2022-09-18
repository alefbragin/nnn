_install-plugins:
    cp -a plugins "/usr/share/nnn/"

make-n-install:
    make O_NERD=1 \
        && make PREFIX=/usr install \
        && install -Dm644 misc/auto-completion/fish/nnn.fish "/usr/share/fish/vendor_completions.d/nnn.fish" \
        && install -Dm644 misc/auto-completion/bash/nnn-completion.bash "/usr/share/bash-completion/completions/nnn" \
        && install -Dm644 misc/auto-completion/zsh/_nnn "/usr/share/zsh/site-functions/_nnn" \
        && install -Dm644 -t "/usr/share/nnn/quitcd/" misc/quitcd/* \
        && just _install-plugins \
        && install -Dm644 -t "/usr/share/licenses/nnn/" LICENSE

install-n-use-plugins:
    just _install-plugins \
    && cp -a "/usr/share/nnn/plugins" "${XDG_CONFIG_HOME:-$HOME/.config}/nnn/"
