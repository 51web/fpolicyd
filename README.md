# flash policy daemon written in C.

A daemon for providing flash policy file.

## how to build rpm

  ```
  git clone https://github.com/51web/fpolicyd fpolicyd-1.0
  tar czf fpolicyd-1.0.tar.gz fpolicyd-1.0
  rpmbuild -tb fpolicyd-1.0.tar.gz
  ```

Get more info about flash policy: http://www.adobe.com/devnet/flashplayer/articles/socket_policy_files.html
