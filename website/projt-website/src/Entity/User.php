<?php

/*

SPDX-License-Identifier: MIT
SPDX-FileCopyrightText: 2026 Project Tick
SPDX-FileContributor: Project Tick

Copyright (c) 2026 Project Tick

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

namespace App\Entity;

use App\Repository\UserRepository;
use Doctrine\Common\Collections\ArrayCollection;
use Doctrine\Common\Collections\Collection;
use Doctrine\ORM\Mapping as ORM;
use Symfony\Component\Security\Core\User\PasswordAuthenticatedUserInterface;
use Symfony\Component\Security\Core\User\UserInterface;

use Symfony\Bridge\Doctrine\Validator\Constraints\UniqueEntity;

#[ORM\Entity(repositoryClass: UserRepository::class)]
#[ORM\UniqueConstraint(name: 'UNIQ_IDENTIFIER_EMAIL', fields: ['email'])]
#[UniqueEntity(fields: ['email'], message: 'There is already an account with this email')]
class User implements UserInterface, PasswordAuthenticatedUserInterface
{
    #[ORM\Id]
    #[ORM\GeneratedValue]
    #[ORM\Column]
    private ?int $id = null;

    #[ORM\Column(length: 180)]
    private ?string $email = null;

    /**
     * @var list<string> The user roles
     */
    #[ORM\Column]
    private array $roles = [];

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $githubId = null;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $githubUsername = null;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $gitlabId = null;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $gitlabUsername = null;

    #[ORM\Column(type: 'text', nullable: true)]
    private ?string $biography = null;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $country = null;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $timezone = null;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $microsoftId = null;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $keycloakId = null;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $minecraftUuid = null;

    #[ORM\Column(type: 'text', nullable: true)]
    private ?string $minecraftAccessToken = null;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $minecraftUsername = null;

    /**
     * @var string The hashed password
     */
    #[ORM\Column(nullable: true)]
    private ?string $password = null;

    #[ORM\OneToMany(mappedBy: 'user', targetEntity: UserLicense::class, orphanRemoval: true)]
    private Collection $userLicenses;

    #[ORM\OneToMany(mappedBy: 'author', targetEntity: DiscussionThread::class)]
    private Collection $threads;

    #[ORM\OneToMany(mappedBy: 'author', targetEntity: DiscussionComment::class)]
    private Collection $comments;

    #[ORM\OneToMany(mappedBy: 'user', targetEntity: PublicCredential::class, orphanRemoval: true)]
    private Collection $publicCredentials;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $resetPasswordToken = null;

    #[ORM\Column(type: 'datetime', nullable: true)]
    private ?\DateTimeInterface $resetPasswordExpiresAt = null;

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $emailChangeToken = null;

    #[ORM\Column(length: 180, nullable: true)]
    private ?string $newEmailPending = null;

    public function __construct()
    {
        $this->userLicenses = new ArrayCollection();
        $this->threads = new ArrayCollection();
        $this->comments = new ArrayCollection();
        $this->publicCredentials = new ArrayCollection();
    }

    /**
     * @return Collection<int, PublicCredential>
     */
    public function getPublicCredentials(): Collection
    {
        return $this->publicCredentials;
    }

    /**
     * @return Collection<int, DiscussionThread>
     */
    public function getThreads(): Collection
    {
        return $this->threads;
    }

    /**
     * @return Collection<int, DiscussionComment>
     */
    public function getComments(): Collection
    {
        return $this->comments;
    }


    #[ORM\Column(length: 255, nullable: true)]
    private ?string $avatar = null;

    #[ORM\Column(length: 180, unique: true, nullable: true)]
    private ?string $username = null;

    public function getAvatar(): ?string
    {
        return $this->avatar;
    }

    public function setAvatar(?string $avatar): static
    {
        $this->avatar = $avatar;

        return $this;
    }

    public function getUsername(): ?string
    {
        return $this->username;
    }

    public function setUsername(?string $username): static
    {
        $this->username = $username;

        return $this;
    }


    public function getId(): ?int
    {
        return $this->id;
    }

    public function getEmail(): ?string
    {
        return $this->email;
    }


    public function setEmail(string $email): static
    {
        $this->email = $email;

        return $this;
    }

    public function getGithubId(): ?string
    {
        return $this->githubId;
    }

    public function setGithubId(?string $githubId): static
    {
        $this->githubId = $githubId;

        return $this;
    }

    public function getGithubUsername(): ?string
    {
        return $this->githubUsername;
    }

    public function setGithubUsername(?string $githubUsername): static
    {
        $this->githubUsername = $githubUsername;

        return $this;
    }

    public function getGitlabId(): ?string
    {
        return $this->gitlabId;
    }

    public function setGitlabId(?string $gitlabId): static
    {
        $this->gitlabId = $gitlabId;

        return $this;
    }

    public function getGitlabUsername(): ?string
    {
        return $this->gitlabUsername;
    }

    public function setGitlabUsername(?string $gitlabUsername): static
    {
        $this->gitlabUsername = $gitlabUsername;

        return $this;
    }

    public function getBiography(): ?string
    {
        return $this->biography;
    }

    public function setBiography(?string $biography): static
    {
        $this->biography = $biography;

        return $this;
    }

    public function getCountry(): ?string
    {
        return $this->country;
    }

    public function setCountry(?string $country): static
    {
        $this->country = $country;

        return $this;
    }

    public function getTimezone(): ?string
    {
        return $this->timezone;
    }

    public function setTimezone(?string $timezone): static
    {
        $this->timezone = $timezone;

        return $this;
    }

    /**
     * A visual identifier that represents this user.
     *
     * @see UserInterface
     */
    public function getUserIdentifier(): string
    {
        return (string) $this->email;
    }

    /**
     * @see UserInterface
     */
    public function getRoles(): array
    {
        $roles = $this->roles;
        // guarantee every user at least has ROLE_USER
        $roles[] = 'ROLE_USER';

        return array_unique($roles);
    }

    /**
     * @param list<string> $roles
     */
    public function setRoles(array $roles): static
    {
        $this->roles = $roles;

        return $this;
    }

    /**
     * @see PasswordAuthenticatedUserInterface
     */
    public function getPassword(): ?string
    {
        return $this->password;
    }

    public function setPassword(string $password): static
    {
        $this->password = $password;

        return $this;
    }

    #[\Deprecated]
    public function eraseCredentials(): void
    {
        // @deprecated, to be removed when upgrading to Symfony 8
    }

    /**
     * @return Collection<int, UserLicense>
     */
    public function getUserLicenses(): Collection
    {
        return $this->userLicenses;
    }

    public function addUserLicense(UserLicense $userLicense): static
    {
        if (!$this->userLicenses->contains($userLicense)) {
            $this->userLicenses->add($userLicense);
            $userLicense->setUser($this);
        }

        return $this;
    }

    public function removeUserLicense(UserLicense $userLicense): static
    {
        if ($this->userLicenses->removeElement($userLicense)) {
            // set the owning side to null (unless already changed)
            if ($userLicense->getUser() === $this) {
                $userLicense->setUser(null);
            }
        }

        return $this;
    }

    #[ORM\Column(length: 255, nullable: true)]
    private ?string $tickApiToken = null;

    public function getTickApiToken(): ?string
    {
        return $this->tickApiToken;
    }

    public function setTickApiToken(?string $tickApiToken): static
    {
        $this->tickApiToken = $tickApiToken;

        return $this;
    }
    public function getResetPasswordToken(): ?string
    {
        return $this->resetPasswordToken;
    }

    public function setResetPasswordToken(?string $resetPasswordToken): static
    {
        $this->resetPasswordToken = $resetPasswordToken;
        return $this;
    }

    public function getResetPasswordExpiresAt(): ?\DateTimeInterface
    {
        return $this->resetPasswordExpiresAt;
    }

    public function setResetPasswordExpiresAt(?\DateTimeInterface $resetPasswordExpiresAt): static
    {
        $this->resetPasswordExpiresAt = $resetPasswordExpiresAt;
        return $this;
    }

    public function getEmailChangeToken(): ?string
    {
        return $this->emailChangeToken;
    }

    public function setEmailChangeToken(?string $emailChangeToken): static
    {
        $this->emailChangeToken = $emailChangeToken;
        return $this;
    }

    public function getNewEmailPending(): ?string
    {
        return $this->newEmailPending;
    }

    public function setNewEmailPending(?string $newEmailPending): static
    {
        $this->newEmailPending = $newEmailPending;
        return $this;
    }

    public function getMicrosoftId(): ?string
    {
        return $this->microsoftId;
    }

    public function setMicrosoftId(?string $microsoftId): static
    {
        $this->microsoftId = $microsoftId;
        return $this;
    }

    public function getKeycloakId(): ?string
    {
        return $this->keycloakId;
    }

    public function setKeycloakId(?string $keycloakId): static
    {
        $this->keycloakId = $keycloakId;
        return $this;
    }

    public function getMinecraftUuid(): ?string
    {
        return $this->minecraftUuid;
    }

    public function setMinecraftUuid(?string $minecraftUuid): static
    {
        $this->minecraftUuid = $minecraftUuid;
        return $this;
    }

    public function getMinecraftAccessToken(): ?string
    {
        return $this->minecraftAccessToken;
    }

    public function setMinecraftAccessToken(?string $minecraftAccessToken): static
    {
        $this->minecraftAccessToken = $minecraftAccessToken;
        return $this;
    }

    public function getMinecraftUsername(): ?string
    {
        return $this->minecraftUsername;
    }

    public function setMinecraftUsername(?string $minecraftUsername): static
    {
        $this->minecraftUsername = $minecraftUsername;
        return $this;
    }
}
