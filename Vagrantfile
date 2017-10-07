# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure(2) do |config|

  config.vm.box = "rasmus/php7dev"

  config.vm.network "forwarded_port", guest: 80, host: 8479

  config.vm.synced_folder ".", "/vagrant", type: "virtualbox"

end
